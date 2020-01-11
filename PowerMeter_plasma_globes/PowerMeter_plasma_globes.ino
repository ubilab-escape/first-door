
#include <SPI.h>
#include "STPM.h"
#include <ESP8266WiFi.h>
// #include <esp_wifi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi_MQTT_Json_Plasma_Slave.h"

#include <EEPROM.h>

#include <ESP8266mDNS.h>

// Arduino Updater
#include <ArduinoOTA.h>

// Serial Speed and DEBUG option
// #define SERIAL_SPEED 2000000
#define SERIAL_SPEED 115200
#define DEBUG


#define MAX_MDNS_LEN 16
#define MDNS_START_ADDRESS 0
// Pins for STPM34 SPI Connection
const int STPM_CS = 15;
const int STPM_SYN = 4;
// Reset pin of STPM
const int STPM_RES = 5;

// Pins for 230V Relay
const int RELAY_PIN = 2;

// STPM Object
STPM stpm34(STPM_RES, STPM_CS, STPM_SYN);


// WLAN configuration
const char *ssids[] = {};
const char *passwords[] = {};

char mdnsName[MAX_MDNS_LEN] = {'\0'};

WiFiUDP udp;

long mdnsUpdate = millis();

//Globe ID
int globeID = 1;
//touch status flags
bool current_touch = 0;
bool old_touch = 0;

//touch recognition
const int numReadings = 25;

//variables for moving average over current samples
float readings[numReadings] = {0};      // current readings
int readIndex = 0;              // the index of the current reading
float total = 0;                  // the running total
float averageInputValue = 0;

float RMScurrent = 0;
float RMSvoltage = 0;

//base reading at the beginning of
int baseline_count = 100; // the number of readings at the beginning whose average sets the baseline 
float baseline = 0;

//factor for recognizing touch
float touch_factor = 0.15;

/************************ SETUP *************************/
void setup() {
  // Set Relay pins as output which should be switched on by default
  pinMode(RELAY_PIN, OUTPUT);
  //turn the plasma globe on initially for baseline reading
  digitalWrite(RELAY_PIN, HIGH);

   // Setup serial communication
  Serial.begin(SERIAL_SPEED);


  // Setup STPM 32
  stpm34.init();

  // Load the MDNS name from eeprom
  EEPROM.begin(2*MAX_MDNS_LEN);

  #ifdef DEBUG
  Serial.println("Info:Connecting WLAN ");
  #endif
  WiFi.mode(WIFI_STA);
  // esp_wifi_set_ps(WIFI_PS_NONE);
  wifi_set_sleep_type(NONE_SLEEP_T);
  char * name = getMDNS();
  WiFi.hostname(name);
  connectNetwork();

  initMDNS();
  setup_mqtt();

  // Let nagle algorithm decide wether to send tcp packets or not or fragment them
  //client.setDefaultNoDelay(false);


  WiFi.setOutputPower(20.5);

  setupOTA();

  mdnsUpdate = millis();
  
  //extract Globe ID from mdns name
  char * eeprom_name = getMDNS();
  String id = "1";
  if (strlen(eeprom_name) == 0) {
    #ifdef DEBUG
    Serial.print(F("No MDNS Name in EEPROM, Globe ID fixed to 1"));
    #endif
  }
  else{
    //globeID = (int) eeprom_name[5];
    //remember char to int (ASCII)
    globeID = eeprom_name[5] - 48;
  }
  //get baseline
  //first current after power on is way higher than baseline --> delay
  delay(2000);
  for(int i = 0; i < baseline_count; i++){
    
      stpm34.readRMSVoltageAndCurrent(1, &RMSvoltage, &RMScurrent);
      baseline += RMScurrent;
      delay(10);
  }
  baseline /= baseline_count;
  //re-initialize the moving average array with the baseline readings
  for(int i = 0; i<numReadings; i++){
    readings[i] = baseline;
    total += baseline;
  }
  
  #ifdef DEBUG
  Serial.print(F("baseline = "));
  Serial.println(baseline);
  #endif

  //turn plasma globe off after baseline reading
  digitalWrite(RELAY_PIN, LOW);

}

// the loop routine runs over and over again forever:
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectNetwork();
    mdnsUpdate = millis();
  }

  // HTTP Updater
  // httpServer.handleClient();
  // Arduino OTA
  ArduinoOTA.handle();

  // Update mdns only on idle
  MDNS.update();
  // Re-advertise service
  if ((long)(millis() - mdnsUpdate) >= 0) {
    MDNS.addService("elec", "tcp", 54322);
    mdnsUpdate += 100000;
  }

  mqtt_loop();
  
  if(globe_status == true){

    //turn on plasma globe
    digitalWrite(RELAY_PIN, HIGH);
    
    //application: touch recognition
    //subtract the last reading:
    total = total - readings[readIndex];
  
    // read from the sensor:
    stpm34.readRMSVoltageAndCurrent(1, &RMSvoltage, &RMScurrent);
    readings[readIndex] = RMScurrent;
    delay(10);
  
    // add the reading to the total:
    total = total + readings[readIndex];
    // advance to the next position in the array:
    readIndex = readIndex + 1;

    // if we're at the end of the array...
    if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
    }

    // Calculate the average input value.
    averageInputValue = total / numReadings;
    //if globe is touched
    if(averageInputValue > (1+touch_factor)*baseline){

      #ifdef DEBUG
      Serial.println("TOUCHED");
      #endif 

      current_touch = 1;
      
      if(current_touch == 1 && old_touch == 0){
        
         mqtt_publish("4/Globes", "message", "touched", String(globeID));
      }
      
      old_touch = 1;
      
    }
    else{

      current_touch = 0;
      if(current_touch == 0 && old_touch == 1){
        
         mqtt_publish("4/Globes", "message", "NOT_touched", String(globeID));
      }
      
      old_touch = 0;
      //Serial.println("NOT TOUCHED");
    }
  //Serial.println(averageInputValue);
  }
  



  yield();
}

void setupOTA() {

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    Serial.println("Start updating");
    // free(buffer);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
    // No matter what happended, simply restart
    WiFi.forceSleepBegin();
    wdt_reset();
    ESP.restart();
    while(1) wdt_reset();
  });

  ArduinoOTA.begin();
}

void connectNetwork() {
  int numKnownNetworks = sizeof(ssids)/sizeof(ssids[0]);
  #ifdef DEBUG
  Serial.print(F("Info:Known Networks: "));
  for (int i = 0; i < numKnownNetworks; i++) {
    Serial.print(ssids[i]);
    if (i < numKnownNetworks-1) Serial.print(", ");
  }
  Serial.println("");
  #endif

  while (WiFi.status() != WL_CONNECTED) {
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    #ifdef DEBUG
    Serial.print(F("Info:Scan done "));
    if (n == 0) {
      Serial.println(F(" no networks found"));
    } else {
      Serial.print(n);
      Serial.println(F(" networks found"));
      for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        Serial.print(F("Info:\t"));
        Serial.print(WiFi.SSID(i));
        Serial.print(F(" ("));
        Serial.print(WiFi.RSSI(i));
        Serial.print(F(")"));
        Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : " *");
      }
    }
    #endif

    if (n != 0) {
      int found = -1;
      int linkQuality = -1000; // The smaller the worse the quality (in dBm)
      for (int i = 0; i < n; ++i) {
        for (int j = 0; j < numKnownNetworks; j++) {
          if (strcmp(WiFi.SSID(i).c_str(), ssids[j]) == 0) {
            if (WiFi.RSSI(i) > linkQuality) {
              linkQuality = WiFi.RSSI(i);
              found = j;
            }
            break;
          }
        }
      }
      if (found != -1) {
        #ifdef DEBUG
        Serial.print(F("Info:Known network with best quality: "));
        Serial.println(ssids[found]);
        #endif
        WiFi.begin(ssids[found], passwords[found]);
        long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
          yield();
          // After trying to connect for 5s continue without wifi
          if (millis() - start > 8000) {
            #ifdef DEBUG
            Serial.print(F("Info:Connection to "));
            Serial.print(ssids[found]);
            Serial.println(F(" failed!"));
            #endif
            break;
          }
        }
      }
    }
    if (WiFi.status() != WL_CONNECTED) delay(1000);
    yield();
  }
#ifdef DEBUG
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Info:Connected"));
    Serial.print(F("Info:SSID:")); Serial.println(WiFi.SSID());
    Serial.print(F("Info:IP Address: ")); Serial.println(WiFi.localIP());
  }
#endif
}







/****************************************************
 * A serial event occured
 ****************************************************/
//void handleEvent(Stream &getter) {
//  if (!getter.available()) return;
//  getter.readStringUntil('\n').toCharArray(command,COMMAND_MAX_SIZE);
//  // Be backwards compatible to "?" command all the time
//  if (command[0] == '?') {
//    getter.println(F("Info:Setup done"));
//    return;
//  }
//  #ifdef DEBUG
//  Serial.print(F("Info:")); Serial.println(command);
//  #endif
//
//  // Deserialize the JSON document
//  DeserializationError error = deserializeJson(docRcv, command);
//  // Test if parsing succeeds.
//  if (error) {
//    getter.print(F("Info:deserializeJson() failed: "));
//    getter.println(error.c_str());
//    #ifdef DEBUG
//    if (&getter != &Serial) {
//      Serial.print(F("Info:deserializeJson() failed: "));
//      Serial.println(error.c_str());
//    }
//    #endif
//    return;
//  }
//  //docSend.clear();
//  JsonObject obj = docSend.to<JsonObject>();
//  obj.clear();
//  response = "";
//  handleJSON();
//  if (docSend.isNull() == false) {
//    getter.flush();
//    response = "";
//    serializeJson(docSend, response);
//    response = "Info:" + response;
//    getter.println(response);
//
//    #ifdef DEBUG
//    if (&getter != &Serial) Serial.println(response);
//    #endif
//  }
//  response = "";
//}

//void handleJSON() {
//  // All commands look like the following:
//  // {"cmd":{"name":"commandName", "payload":{<possible data>}}}
//  // e.g. mdns
//
//  const char* cmd = docRcv[F("cmd")]["name"];
//  JsonObject root = docRcv.as<JsonObject>();
//  if (cmd == nullptr) {
//    docSend["msg"] = F("JSON does not match format, syntax: {\"cmd\":{\"name\":<commandName>, \"[payload\":{<possible data>}]}}");
//    return;
//  }
//
//  /*********************** SAMPLING COMMAND ****************************/
//  // e.g. {"cmd":{"name":"sample", "payload":{"type":"Serial", "rate":4000}}}
//  if(strcmp(cmd, CMD_SAMPLE) == 0) {
//    // For sampling we need type payload and rate payload
//    const char* typeC = root["cmd"]["payload"]["type"];
//    const char* measuresC = root["cmd"]["payload"]["measures"];
//    int rate = docRcv["cmd"]["payload"]["rate"].as<int>();
//    unsigned long ts = docRcv["cmd"]["payload"]["time"].as<unsigned long>();
//
//    docSend["error"] = true;
//    if (typeC == nullptr or rate == 0) {
//      response = "Not a valid \"sample\" command";
//      if (typeC == nullptr) response += ", \"type\" missing";
//      if (rate == 0) response += ", \"rate\" missing";
//      docSend["msg"] = response;
//      return;
//    }
//    if (rate > 8000 || rate <= 0) {
//      response = "SamplingRate could not be set to ";
//      response += rate;
//      docSend["msg"] = response;
//      return;
//    }
//    if (measuresC == nullptr) {
//      measures = STATE_VI;
//      MEASURMENT_BYTES = 8;
//    } else if (strcmp(measuresC, "v,i") == 0) {
//      measures = STATE_VI;
//      MEASURMENT_BYTES = 8;
//    } else if (strcmp(measuresC, "p,q") == 0) {
//      measures = STATE_PQ;
//      MEASURMENT_BYTES = 8;
//    } else if (strcmp(measuresC, "v,i,p,q") == 0) {
//      measures = STATE_VIPQ;
//      MEASURMENT_BYTES = 16;
//    } else {
//      response = "Unsupported measures";
//      response += measuresC;
//      docSend["msg"] = response;
//      return;
//    }
//    // e.g. {"cmd":{"name":"sample", "payload":{"type":"Serial", "rate":4000}}}
//    if (strcmp(typeC, "Serial") == 0) {
//      next_state = STATE_SAMPLE_USB;
//    // e.g. {"cmd":{"name":"sample", "payload":{"type":"TCP", "rate":4000}}}
//    } else if (strcmp(typeC, "TCP") == 0) {
//      next_state = STATE_SAMPLE_TCP;
//    // e.g. {"cmd":{"name":"sample", "payload":{"type":"UDP", "rate":4000}}}
//    } else if (strcmp(typeC, "UDP") == 0) {
//      next_state = STATE_SAMPLE_UDP;
//      int port = docRcv["cmd"]["payload"]["port"].as<int>();
//      if (port > 80000 || port <= 0) {
//        udpPort = STANDARD_UDP_PORT;
//        response = "Unsupported UDP port";
//        response += port;
//        docSend["msg"] = response;
//        return;
//      } else {
//        udpPort = port;
//      }
//      docSend["port"] = udpPort;
//    } else if (strcmp(typeC, "FFMPEG") == 0) {
//      bool success = tryConnectStream();
//      if (success) {
//        next_state = STATE_STREAM_TCP;
//        response = F("Connected to TCP stream");
//      } else {
//        docSend["msg"] = F("Could not connect to TCP stream");
//        return;
//      }
//    } else {
//      response = F("Unsupported sampling type: ");
//      response += typeC;
//      docSend["msg"] = response;
//      return;
//    }
//    // Set global sampling variable
//    samplingRate = rate;
//    TIMER_CYCLES_FAST = (CLOCK * 1000000) / samplingRate; // Cycles between HW timer inerrupts
//    calcChunkSize();
//
//    docSend["sampling_rate"] = samplingRate;
//    docSend["chunk_size"] = chunkSize;
//    docSend["conn_type"] = typeC;
//    docSend["timer_cycles"] = TIMER_CYCLES_FAST;
//    docSend["cmd"] = CMD_SAMPLE;
//
//    if (ts != 0) {
//      response += F("Should sample at: ");
//      response += printTime(ts,0);
//      updateTime(true);
//      uint32_t delta = ts - currentSeconds;
//      uint32_t nowMs = millis();
//      delta *= 1000;
//      delta -= currentMilliseconds;
//      if (delta > 20000 or delta < 500) {
//        response += F("//nCannot start sampling in: "); response += delta; response += F("ms");
//        samplingCountdown = 0;
//      } else {
//        response += F("//nStart sampling in: "); response += delta; response += F("ms");
//        samplingCountdown = nowMs + delta;
//        docSend["error"] = false;
//      }
//      docSend["msg"] = String(response);
//      return;
//    }
//    docSend["error"] = false;
//    state = next_state;
//    // UDP packets are not allowed to exceed 1500 bytes, so keep size reasonable
//    startSampling(true);
//  }
//
//  /*********************** SWITCHING COMMAND ****************************/
//  else if (strcmp(cmd, CMD_SWITCH) == 0) {
//    // For switching we need value payload
//    docSend["error"] = true;
//    const char* payloadValue = docRcv["cmd"]["payload"]["value"];
//    if (payloadValue == nullptr) {
//      docSend["msg"] = F("Info:Not a valid \"switch\" command");
//      return;
//    }
//    bool value = docRcv["cmd"]["payload"]["value"].as<bool>();
//    docSend["msg"]["switch"] = value;
//    response = F("Switching: ");
//    response += value ? F("On") : F("Off");
//    docSend["msg"] = response;
//    docSend["error"] = false;
//    switchRelay(RELAY_PIN, value);
//  }
//
//  /*********************** STOP COMMAND ****************************/
//  // e.g. {"cmd":{"name":"stop"}}
//  else if (strcmp(cmd, CMD_STOP) == 0) {
//    // State is reset in stopSampling
//    int prev_state = state;
//    stopSampling();
//    // Send remaining
//    if (prev_state == STATE_SAMPLE_USB) writeChunks(Serial, true);
//    else if (prev_state == STATE_SAMPLE_TCP) writeChunks(client, true);
//    else if (prev_state == STATE_SAMPLE_UDP) {
//      udp.beginPacket(udpIP, udpPort);
//      writeChunks(udp, true);
//      udp.endPacket();
//    }
//    docSend["msg"] = F("Received stop command");
//    docSend["sample_duration"] = samplingDuration;
//    docSend["samples"] = totalSamples;
//    docSend["sent_samples"] = sentSamples;
//    docSend["avg_rate"] = totalSamples/(samplingDuration/1000.0);
//    docSend["cmd"] = CMD_STOP;
//  }
//
//  /*********************** RESTART COMMAND ****************************/
//  // e.g. {"cmd":{"name":"restart"}}
//  else if (strcmp(cmd, CMD_RESTART) == 0) {
//    stopSampling();
//    WiFi.forceSleepBegin();
//    wdt_reset();
//    ESP.restart();
//    while(1) wdt_reset();
//  }
//
//  /*********************** INFO COMMAND ****************************/
//  // e.g. {"cmd":{"name":"info"}}
//  else if (strcmp(cmd, CMD_INFO) == 0) {
//    docSend["msg"] = F("WIFI powermeter");
//    docSend["version"] = VERSION;
//    docSend["buffer_size"] = BUF_SIZE;
//    docSend["name"] = mdnsName;
//    docSend["sampling_rate"] = samplingRate;
//  }
//
//  /*********************** MDNS COMMAND ****************************/
//  // e.g. {"cmd":{"name":"mdns", "payload":{"name":"newName"}}}
//  else if (strcmp(cmd, CMD_MDNS) == 0) {
//    docSend["error"] = true;
//    const char* newName = docRcv["cmd"]["payload"]["name"];
//    if (newName == nullptr) {
//      docSend["msg"] = F("MDNS name required in payload with key name");
//      return;
//    }
//    if (strlen(newName) < MAX_MDNS_LEN) {
//      writeMDNS((char * )newName);
//    } else {
//      response = F("MDNS name too long, only string of size ");
//      response += MAX_MDNS_LEN;
//      response += F(" allowed");
//      docSend["msg"] = response;
//      return;
//    }
//    char * name = getMDNS();
//    response = F("Set MDNS name to: ");
//    response += name;
//    //docSend["msg"] = sprintf( %s", name);
//    docSend["msg"] = response;
//    docSend["mdns_name"] = name;
//    docSend["error"] = false;
//    initMDNS();
//  }
//
//  /*********************** NTP COMMAND ****************************/
//  // e.g. {"cmd":{"name":"ntp"}}
//  else if (strcmp(cmd, CMD_NTP) == 0) {
//    if (getTimeNTP()) {
//      docSend["msg"] = "Time synced";
//      docSend["error"] = false;
//    } else {
//      docSend["msg"] = "Error syncing time";
//      docSend["error"] = true;
//    }
//    char * timeStr = printTime(ntpEpochSeconds, ntpMilliseconds);
//    docSend["ntp_time"] = timeStr;
//    timeStr = printCurrentTime();
//    docSend["current_time"] = timeStr;
//  }
//}




/****************************************************
 * Switch relay to open or closed
 ****************************************************/
inline void switchRelay(int number, bool value) {
  digitalWrite(number, value);
}


/****************************************************
 * Init the MDNs name from eeprom, only the number ist
 * stored in the eeprom, construct using prefix.
 ****************************************************/
void initMDNS() {
  char * name = getMDNS();
  if (strlen(name) == 0) {
    Serial.print(F("No MDNS Name in EEPROM, assign default"));
    strcpy(name,"Globe1");
  }
  // Setting up MDNs with the given Name
  Serial.print(F("Info:MDNS Name: ")); Serial.println(name);
  if (!MDNS.begin(String(name).c_str())) {             // Start the mDNS responder for esp8266.local
    Serial.println(F("Info:Error setting up MDNS responder!"));
  }

}

char * getMDNS() {
  uint16_t address = MDNS_START_ADDRESS;
  uint8_t chars = 0;
  EEPROM.get(address, chars);
  address += sizeof(chars);
  if (chars < MAX_MDNS_LEN) {
    EEPROM.get(address, mdnsName);
  }
  return mdnsName;
}

void writeMDNS(const char * newName) {
  uint16_t address = MDNS_START_ADDRESS;
  uint8_t chars = strlen(newName);
  EEPROM.put(address, chars);
  address += sizeof(chars);
  for (uint8_t i = 0; i < chars; i++) EEPROM.put(address+i, newName[i]);
  EEPROM.put(address+chars, '\0');
  EEPROM.commit();
}
