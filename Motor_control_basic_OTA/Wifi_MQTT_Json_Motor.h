#ifndef WiFi_MQTT_Json_Motor_H
#define WiFi_MQTT_Json_Motor_H


//variables
long mdnsUpdate = millis();

bool openDoor = false;
bool closeDoor = false;

//function declarations
void setup_wifi();
void setup_mqtt();
void initMDNS();
void setupOTA();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
void mqtt_publish(String topic_name, String Method, String State, String Data = "");

#endif /* WiFi_MQTT_Json_Motor_H */
