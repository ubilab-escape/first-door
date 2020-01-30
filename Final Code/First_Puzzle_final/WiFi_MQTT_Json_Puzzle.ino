/*
 First Puzzle

 
 This code 
 - establishes Wifi connection
 - establishes MQTT protocoll
 - handles JSON communication to server/ other uC
 - enables Over The Air updates (OTA)
 
 - JSON messages: {method, state}


*/
#include "WiFi_MQTT_Json_Puzzle.h"

//EEPROM addressing
#define MAX_MDNS_LEN 16
#define MDNS_START_ADDRESS 0

//mdns - broadcasting name for OTA
char mdnsName[MAX_MDNS_LEN] = {'\0'};

//Wifi variables
/*
// Set your Static IP address
IPAddress local_IP(10, 0, 4, 0);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
*/

const char* ssid = "";
const char* password = "";

//MQTT variables
//const char* mqtt_server = "broker.mqtt-dashboard.com";
//IP address of computer which runs mqtt server (broker) --> mqtt main server ip: 10,0,0,2 / PC:192,168,178,82
const IPAddress mqttServerIP(10,0,0,2);
//mqtt device id
const String clientId = "4/puzzle";

WiFiClient wifiClient;
PubSubClient client(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;


//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function setup_wifi
 * param: none
 * return: none
 * purpose: configure WiFi and connect to network
 */
void setup_wifi() {

  /*// Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }*/

  delay(10);
  // We start by connecting to a WiFi network
  #ifdef DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  delay(300);
  #endif

  WiFi.mode(WIFI_STA);
  delay(500);
  WiFi.begin(ssid, password);
  delay(1000);
  WiFi.begin(ssid, password);
  long timer = millis();
  
  while( millis() - timer < 10000) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(5000);
    Serial.println(".");
    WiFi.begin(ssid, password);
  }

  if  (WiFi.status() != WL_CONNECTED) {
    #ifdef DEBUG
    delay(500);
    Serial.print("Connection Failed rebooting...");
    #endif
    delay(100);
    ESP.restart();
  }
  
  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif DEBUG
}


//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function setup_mqtt
 * param: none
 * return: none
 * purpose: define mqtt broker (server) and set callback fct to receive messages published to subscribed topics
 */
void setup_mqtt(){
  
  client.setServer(mqttServerIP, 1883);
  client.setCallback(mqtt_callback);
}


//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function mqtt_callback
 * param: -topic: name of the topic wich has a new message
 *        -payload: the message itself
 *        -length: length of the message
 * return: none
 * purpose: Callback function is fired each time a message is published in any of the subsribed topics
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  
  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
  
  //Deserialize
  StaticJsonDocument<300> doc;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  //first puzzle controller handles only method and state
  String Method = doc["method"];
  String State = doc["state"];
  String Data = doc["data"]; //should be 0 if not data argument was received (see https://arduinojson.org/v6/doc/deserialization/ --> 3.3.3 in pdf)
  
  //Act according to trigger value
  /*********************** trigger: on ****************************/
  if(Method == "trigger" && State == "on"){

    //set puzzle state variable
    puzzle_state = true;
    //respond to activation message
    //{"method":"STATUS","state":"active"}
    mqtt_publish("4/puzzle", "STATUS", "active");

    //call activate_puzzle function
  }
  
  /*********************** change mdns name ****************************/
  else if (Method == "message" && State == "mdns_change"){
    
    #ifdef DEBUG
    Serial.println(F("mdns name change requested"));
    #endif
    //adapt mdns name in eeprom (flash) to the string in Data variable
    //change gets visible only after restart of esp32 (when mdns name is initialized)
    writeMDNS(Data.c_str());    
  }

}


//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function mqtt_loop
 * param: none
 * return: none
 * purpose: this function sets up the mqtt connection and should be called regularly in order 
 *          to process incoming messages and maintain connection to mqtt server --> CALL IT IN MAIN LOOP!
 */
void mqtt_loop() {

  while (!client.connected()) {
    
    #ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(mdnsName)) {
      #ifdef DEBUG
      Serial.println("connected");
      #endif
      /*// Once connected, publish an announcement...
      client.publish("outTopic", "hello world");*/
      // ... and resubscribe
      client.subscribe("4/puzzle");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
  client.loop();
  /*
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 50, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
  }*/
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function mqtt_publish
 * param: -topic_name: name of the topic to which the message is to be published
 *        -Method: JSON-value for JSON-name method
 *        -State: JSON-value for  JSON-name state
 *        -Data: JSON-value for JSON-name data [OPTIONAL] --> default value is set in function declaration (see .h file!)
 * return: none
 * purpose: funtion packs message into JSON format and publishes to specified MQTT topic
 */
//alternative header for data ARRAY: void mqtt_publish(String topic_name, String Method, String State, int data_array[] = {}, int length_of_array = 0){

void mqtt_publish(String topic_name, String Method, String State, int Data){
  //Create the JSON format
  
  //const int capacity = JSON_ARRAY_SIZE(length_of_array) + 2*JSON_OBJECT_SIZE(3)
  //allocate a Json Document
  StaticJsonDocument<300> doc;
  doc["method"] = Method;
  doc["state"] = State;

  /*if(length_of_array != 0){
      JsonArray data = doc.createNestedArray("data");
      
      for(int i = 0; i < length_of_array; i++){
        data.add(data_array[i]);
      }
    }
  */
  //in case a data (integer value) was handed as parameter
  if(Data != -9999){
    doc["data"] = Data;
  }

  //create a buffer that holds the serialized JSON message
  int msg_length = measureJson(doc) + 1;                    //measureJson return value doesn't count the null-terminator
  char JSONmessageBuffer[msg_length];                         
  // Generate the minified JSON and send it to the Serial port.
  #ifdef DEBUG
  Serial.print("JSON message created for publishing: ");
  serializeJson(doc, Serial);
  Serial.println();
  #endif
  // Generate the minified JSON and save it in the message buffer
  serializeJson(doc, JSONmessageBuffer, sizeof(JSONmessageBuffer));

  //send the JSON message to the specified topic
  if (client.publish(topic_name.c_str(), JSONmessageBuffer) == false) {
    Serial.println("Error sending message");
  }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
/****************************************************
 * Init the MDNs name from eeprom, only the number ist
 * stored in the eeprom, construct using prefix.
 ****************************************************/
void initMDNS() {
  // Load the MDNS name from eeprom
  EEPROM.begin(2*MAX_MDNS_LEN);
  
  char * name = getMDNS();
  if (strlen(name) == 0) {
    Serial.println(F("Info:Sth wrong with mdns"));
    //use clientID as default mdns name
    strcpy(name, clientId.c_str());
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

//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function setupOTA
 * param: none
 * return: none
 * purpose: establish OTA 
 */
void setupOTA() {

  // No authentication by default
  ArduinoOTA.setHostname("Keypad");
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
    //WiFi.forceSleepBegin();
    //wdt_reset();
    ESP.restart();
    //while(1) wdt_reset();
  });

  ArduinoOTA.begin();
}
