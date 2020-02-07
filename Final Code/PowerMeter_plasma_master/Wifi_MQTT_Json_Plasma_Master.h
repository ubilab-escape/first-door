#ifndef WiFi_MQTT_Json_Plasma_Master_H
#define WiFi_MQTT_Json_Plasma_Master_H


//variables
bool globe_status = false;
//slave globe states (use int for summing up)
int globe_0 = 0;
int globe_1 = 0;
int globe_2 = 0;
int globe_3 = 0;
//number of participants
int participants = 0;

//function declarations
void setup_wifi();
void setup_mqtt();
void initMDNS();
void setupOTA();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
void mqtt_publish(String topic_name, String Method, String State, String Data = "");

#endif /* WiFi_MQTT_Json_Plasma_Master_H */
