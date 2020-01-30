/*
 Doors

 This code 
 - establishes Wifi connection
 - establishes MQTT protocoll
 - handles JSON communication to server/ other uC
 - enables Over The Air updates (OTA)
*/
#include "WiFi_MQTT_Json_Motor.h"

#define DEBUG

//EEPROM addressing
#define MAX_MDNS_LEN 16
#define MDNS_START_ADDRESS 0
#define MAX_WIFI_LEN 32
#define WIFI_SSID_START_ADDRESS 32
#define WIFI_PWD_START_ADDRESS 64

//mdns - broadcasting name for OTA
char mdnsName[MAX_MDNS_LEN] = {'\0'};

//Wifi credentials 
char ssid[] = "";
char password[] = "";
//char ssid[] = "";
//char password[] = "";

//MQTT variables
//IP address of computer which runs mqtt server (broker) --> mqtt main server ip: 10,0,0,2
const IPAddress mqttServerIP(10,0,0,2);
//mqtt device id
#ifdef DOOR1
const String clientId = "Group4_Door_Entrance";
#endif
#ifdef DOOR2
const String clientId = "Group4_Door_Server";
#endif

String mdns_command = "mdns_change:";

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
  
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  
  //2x WiFi.begin necessary for connection
  WiFi.begin(ssid, password);
  delay(1000);
  WiFi.begin(ssid, password);

  //try connecting 
  long timer = millis();
  while (millis() - timer < 10000) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(5000);
    #ifdef DEBUG
    Serial.print(".");
    #endif 
    WiFi.begin(ssid, password);
  }
  
  //restart ESP if connection failed
  if (WiFi.status() != WL_CONNECTED) {
    #ifdef DEBUG
    Serial.println("Connection Failed! Rebooting...");
    #endif 
    delay(100);
    ESP.restart();
  }

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
/*
 * function setupOTA
 * param: none
 * return: none
 * purpose: establish OTA 
 */
void setupOTA() {
  
  // Hostname defaults to esp3232-[MAC]
  #ifdef DOOR1
  ArduinoOTA.setHostname("Door1");
  #endif
  #ifdef  DOOR2
  ArduinoOTA.setHostname("Door2");
  #endif

  ArduinoOTA.setPassword("group4");

  ArduinoOTA.onStart([]()
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    #ifdef DEBUG
    Serial.println("Start updating " + type);    
    #endif 
  })
  .onEnd([]()
  {
    #ifdef DEBUG
    Serial.println("\nEnd");    
    #endif 
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    #ifdef DEBUG
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));    
    #endif 
  })
  .onError([](ota_error_t error)
  {
    #ifdef DEBUG
    Serial.printf("Error[%u]: ", error);    
    #endif 
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
    #ifdef DEBUG
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());    
    #endif 
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
  /*
  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
  */
  
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

  String Method = doc["method"];
  String State = doc["state"];
  String Data = doc["data"]; //should be 0 if no data argument was received (see https://arduinojson.org/v6/doc/deserialization/ --> 3.3.3 in pdf)
  
  //Act according to trigger value
  /*********************** trigger: on ****************************/
  if(Method == "trigger" && State == "on"){

    //set opening flag
    openDoor = true;
    closeDoor = false;

    //respond to activation message
    //{"method":"STATUS","state":"active", "data":"opening"}
    //mqtt_publish("4/door/entrance", "status", "active", "opening");

  }
  /*********************** trigger: off ****************************/
  if(Method == "trigger" && State == "off"){

    //set closing flag
    closeDoor = true;
    openDoor = false;

    //respond to activation message
    //{"method":"STATUS","state":"active", "data":"closing"}
    //mqtt_publish("4/door/entrance", "status", "active", "closing");

  }
  
  /*********************** change mdns name ****************************/
  /*MDNS stuff not used in door code  
  else if (Method == "message" && State == (mdns_command + mdnsName)){
    
    #ifdef DEBUG
    Serial.println(F("mdns name change requested"));
    #endif
    
    //adapt mdns name in eeprom (flash) to the string in Data variable
    //This also changes the MQTT client name!
    writeMDNS(Data.c_str());
    #ifdef DEBUG
    Serial.print(F("New OTA and MQTT name: "));
    Serial.println(Data.c_str());
    Serial.print("Restarting to see changed name...");
    delay(1000);   
    #endif

    //restart the esp to see changed name in OTA
    ESP.restart();  
  }
  */
  /*********************** reset esp ****************************/  
  else if (Method == "message" && State == "restart"){
    ESP.restart();

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
      #ifdef DOOR1
      client.subscribe("4/door/entrance");
      #endif
      #ifdef DOOR2
      client.subscribe("4/door/server");
      #endif
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

void mqtt_publish(String topic_name, String Method, String State, String Data){
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
  if(Data != ""){
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
/*
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
*/
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//***save/load the wifi credentials to/ from eeprom
void loadCredentials() {
  EEPROM.begin(512);
  
  /*
  EEPROM.get(MAX_MDNS_LEN, ssid);
  Serial.print("read ssid: ");
  Serial.println(ssid);
  EEPROM.get(MAX_MDNS_LEN+sizeof(ssid), password);
  char ok[2+1];
  EEPROM.get(MAX_MDNS_LEN+sizeof(ssid)+sizeof(password), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
  }
  Serial.println("Recovered credentials:");
  //Serial.println(ssid);
  Serial.println(strlen(password)>0?"********":"<no password>");
  */

  uint16_t address = WIFI_SSID_START_ADDRESS;
  uint8_t chars = 0;
  EEPROM.get(address, chars);
  address += 1;
  if (chars < MAX_WIFI_LEN) {
    for (uint8_t i = 0; i < chars+1; i++) EEPROM.get(address+i, ssid[i]);
  }
  //int leng = strlen(ssid);
  //ssid[leng+1] = '\0';
  Serial.print("ssid length: ");
  Serial.println(strlen(ssid));
  Serial.print("read ssid: ");
  for (uint8_t i = 0; i <= strlen(ssid); i++){
  Serial.print(ssid[i]);
  }
  Serial.println("");

  address = WIFI_PWD_START_ADDRESS;
  chars = 0;
  EEPROM.get(address, chars);
  address += sizeof(chars);
  if (chars < MAX_WIFI_LEN) {
    for (uint8_t i = 0; i < chars+1; i++) EEPROM.get(address+i, password[i]);
  }
  Serial.print("pwd length: ");
  Serial.println(strlen(password));
  Serial.print("read pwd: ");
  for (uint8_t i = 0; i <= strlen(ssid); i++){
  Serial.print(password[i]);
  }
  Serial.println("");

  
}

//-----------------------------------------------------------------------
void saveCredentials() {
  
  uint16_t address = WIFI_SSID_START_ADDRESS;
  uint8_t chars = strlen(ssid);
  
  EEPROM.begin(512);
  EEPROM.put(address, chars);
  address += 1;
  for (uint8_t i = 0; i < chars; i++) EEPROM.put(address+i, ssid[i]);
  EEPROM.put(address+chars, '\0');

  address = WIFI_PWD_START_ADDRESS;
  chars = strlen(password);
  EEPROM.put(address, chars);
  address += 1;
  for (uint8_t i = 0; i < chars; i++) EEPROM.put(address+i, password[i]);
  EEPROM.put(address+chars, '\0');
  
  /*
  EEPROM.put(MAX_MDNS_LEN, ssid);
  Serial.print("write in eeprom (ssid): ");
  Serial.println(ssid);
  EEPROM.put(MAX_MDNS_LEN+sizeof(ssid), password);
  char ok[2+1] = "OK";
  EEPROM.put(MAX_MDNS_LEN+sizeof(ssid)+sizeof(password), ok);
  */
  EEPROM.commit();
  EEPROM.end();
}
