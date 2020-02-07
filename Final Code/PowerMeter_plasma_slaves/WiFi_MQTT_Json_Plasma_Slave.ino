/*
 Plasma globe slaves

 
 This code 
 - establishes MQTT protocoll
 - handles JSON communication to server/ other uC
 
 - JSON messages: {method, state}


*/
#include "WiFi_MQTT_Json_Plasma_Slave.h"

//MQTT variables
//const char* mqtt_server = "broker.mqtt-dashboard.com";
//IP address of computer which runs mqtt server (broker) --> mqtt main server ip: 10,0,0,2
const IPAddress mqttServerIP(10,0,0,2);
//mqtt device id
//const String clientId = "Group4_Plasma_Slave";

// 
String mdns_command = "mdns_change:";

WiFiClient wifiClient;
PubSubClient client(wifiClient);


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

  String Method = doc["method"];
  String State = doc["state"];
  String Data = doc["data"]; //should be 0 if not data argument was received (see https://arduinojson.org/v6/doc/deserialization/ --> 3.3.3 in pdf)
  
  //Act according to trigger value
  /*********************** trigger: on ****************************/
  if(Method == "trigger" && State == "on"){

    //get number of participants
    if(Data.toInt() > globeID){
      globe_status = true;
      //turn on plasma globe
      digitalWrite(RELAY_PIN, HIGH);
      
      //respond to activation message
      //{"method":"STATUS","state":"active"}
      mqtt_publish("4/globes", "STATUS", "active", String(globeID));
    
      //first current after power on is way higher than baseline --> delay
      delay(2000);
    }
    else{
    
      globe_status = false;
      //turn the plasma globe off
      digitalWrite(RELAY_PIN, LOW);
      
    }

    //call activate_puzzle function
  }
  
  /*********************** change mdns name ****************************/
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

/*********************** restart esp ****************************/  
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
    // Attempt to connect with mdns name
    if (client.connect(mdnsName)) {
      #ifdef DEBUG
      Serial.println("MQTT connected!");
      #endif
      /*// Once connected, publish an announcement...
      client.publish("outTopic", "hello world");*/
      // ... and resubscribe
      client.subscribe("4/globes");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 second");
      // Wait 1 second before retrying
      delay(1000);
    }
  }
  
  client.loop();
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
void mqtt_publish(String topic_name, String Method, String State, String Data){
  //Create the JSON format
  
  //const int capacity = JSON_ARRAY_SIZE(length_of_array) + 2*JSON_OBJECT_SIZE(3)
  //allocate a Json Document
  StaticJsonDocument<300> doc;
  doc["method"] = Method;
  doc["state"] = State;

  //in case a data (String) was handed as parameter
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
