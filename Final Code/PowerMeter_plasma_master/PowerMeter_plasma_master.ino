
#include <SPI.h>
#include "STPM.h"
#include <ESP8266WiFi.h>
// #include <esp_wifi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi_MQTT_Json_Plasma_Master.h"

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

//door status: open = 1, close = 0
bool door_status = 1;

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
    //globeID = (int) eeprom_name[5]
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
      //only send touched message if this globe was NOT touched before
      if(current_touch == 1 && old_touch == 0){
        
         mqtt_publish("4/globes", "message", "touched", String(globeID));
         globe_0 = 1;
      }
      
      old_touch = 1;
      
    }
    else{

      current_touch = 0;
      //only send NOT_touched message if this globe was touched before
      if(current_touch == 0 && old_touch == 1){
        
         mqtt_publish("4/globes", "message", "NOT_touched", String(globeID));
         globe_0 = 0;
      }
      
      old_touch = 0;
      //Serial.println("NOT TOUCHED");
    }

    //check if all globes are touched and door is open currently /in opening process
    if(((globe_0 + globe_1 + globe_2 + globe_3) == participants) && door_status == 1){

      //all globes are touched! --> close the door
      mqtt_publish("4/door/entrance", "trigger", "off");
      door_status = 0;
    }
    //if not all globes are touched and door is closed currently/ in closing process
    else if(((globe_0 + globe_1 + globe_2 + globe_3) < participants) && door_status == 0){

      //NOT all globes are touched! --> close the door
      mqtt_publish("4/door/entrance", "trigger", "on");
      door_status = 1;
    }
  
  }
  
  yield();
}

void setupOTA() {

  // No authentication by default
  ArduinoOTA.setPassword("group4");

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
