#ifndef WiFi_MQTT_Json_Puzzle_H
#define WiFi_MQTT_Json_Puzzle_H


//variables
long mdnsUpdate = millis();
bool puzzle_state = false;

//function declarations
void setup_wifi();
void setup_mqtt();
void initMDNS();
void setupOTA();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();
void mqtt_publish(String topic_name, String Method, String State, int Data = -9999);

#endif /* WiFi_MQTT_Json_Puzzle_H */
