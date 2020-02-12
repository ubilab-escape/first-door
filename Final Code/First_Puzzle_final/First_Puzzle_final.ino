/*
    Author: Sarah Eisenkolb
    Description: Numbpad and LCD
    PIN MC | Connected to Numpad      PIN MC | Connected to LCD
    16     | 1                        GND    | GND
    17     | 2                        5V     | VCC
    32     | 3                        22     | SCL
    33     | 4                        21     | SDA
    25     | 5
    26     | 6
    27     | 7
    14     | 8
*/

#include "WiFi_MQTT_Json_Puzzle.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DEBUG
//**********************Variables*************************
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

//lcd line characters
char riddle_text[17] = "*** Riddle 1 ***";
char wrong_code_text[17] = "***Wrong Code***";
char right_code_text[17] = "***Right Code***";
char empty_line[17] = "                ";

//Initialisation Keypad
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
const byte PWNUMB = 4; // six digit number

//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {27,14,17,16}; //connect to the row pinouts of the keypad 
byte colPins[COLS] = {32,33,25,26}; //connect to the column pinouts of the keypad 
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// global variables
bool puzzle_init = false;
int numb = 0;
int blinkit = 0;
char myInput[4];
char myPasswort[] = "1956";


//************************Setup Function**************************************
void setup() {
  
  Serial.begin(115200);

  //Communication setup
  setup_wifi();
  setup_mqtt();
  setupOTA();
  initMDNS();
  mdnsUpdate = millis();

  //Application setup
  // initialize the LCD
  lcd.begin();
  //turn off backlight
  lcd.noBacklight();
  
}

void loop(){
  //reconnect to Wifi
    if (WiFi.status() != WL_CONNECTED) {
      setup_wifi();
  }
  
  // Arduino OTA
  ArduinoOTA.handle();


  // Re-advertise service
  if ((long)(millis() - mdnsUpdate) >= 0) {
    MDNS.addService("elec", "tcp", 54322);
    mdnsUpdate += 100000;
  }
  
  //(re-) connect to mqtt server and check for incoming messages
  mqtt_loop();


  //***Application loop***
  //if trigger on message arrived and puzzle was off before, turn lcd on
  if(puzzle_state == true && puzzle_init == false){
    
    #ifdef DEBUG
    Serial.println(F("puzzle activated"));
    #endif

    lcd.begin();
    
    puzzle_init = true;
    Serial.println(puzzle_init);
    // Turn on the blacklight and print a message.
    //lcd.clear();
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print(riddle_text);
    lcd.print(empty_line);
    // Wait 3 seconds
    delay(3000);
    lcd.setCursor(0,0);
    lcd.print(empty_line);
    lcd.print(empty_line);
    lcd.setCursor(0,0);
    delay(500);   
  }

  //check keypad input
  if(puzzle_state == true){
    
    //get input character of keypad
    char customKey = customKeypad.getKey();
    
    //if something was typed in
    if (customKey) {
      //write corresponding character to current lcd position
      lcd.setCursor(numb,0);
      myInput[numb] = customKey;
      lcd.print(customKey);
      //write empty line in 2nd lcd line
      lcd.setCursor(0,1);
      lcd.print(empty_line);
      //increase number of typed characters
      numb++;
      
      //if 4 digits entered
      if (numb == PWNUMB ) {
        delay(300);
        #ifdef DEBUG
        Serial.print("entered passwort: ");
        Serial.println(myInput);
        #endif
        
        //if password correct
        if (strncmp(myInput, myPasswort,4) == 0) {
          
          // send puzzle solved message
          mqtt_publish("4/puzzle", "status", "solved");

          //print code accepted to lcd
          delay(200);
          lcd.setCursor(0,0);
          lcd.print(right_code_text);
          lcd.print(empty_line);
          delay(1000);
          //turn off lcd
          lcd.noBacklight();
          lcd.setCursor(0,0);
          lcd.print(empty_line);
          lcd.print(empty_line);
          numb = 0;
          puzzle_state = false;
          puzzle_init = false;
        }

        //else: wrong password --> blink lcd
        else {
          blinkit = 0;
          lcd.setCursor(0,0);
          lcd.print(wrong_code_text);
          while (blinkit < 4) {
            lcd.noBacklight();
            delay(200);
            lcd.backlight();
            delay(200);
            myInput[blinkit] = '\0';
            blinkit++;
          }
          //clear lcd and set cursor back to start
          lcd.setCursor(0,0);
          lcd.print(empty_line);
          lcd.print(empty_line);
          numb = 0;
        }
      }
    }
  }
  
}
