#ifndef WiFi_MQTT_Json_Plasma_Slave_H
#define WiFi_MQTT_Json_Plasma_Slave_H


//variables
bool globe_status = false;

//function declarations
void setup_wifi();
void setup_mqtt();
void initMDNS();
void setupOTA();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
void mqtt_publish(String topic_name, String Method, String State, String Data = "");

#endif /* WiFi_MQTT_Json_Plasma_Slave_H */
