//www.elegoo.com
//2016.12.9

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

//  Includes
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#include <WiFi.h>
//#include "Esp32MQTTClient.h"

// Please input the SSID and password of WiFi
const char* ssid     = "";
const char* password = "";

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Initialisation Keypad
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
const byte PWNUMB = 4; // four digit number

//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {25, 26, 27, 14}; //connect to the row pinouts of the keypad // 25, 26, 27, 14
byte colPins[COLS] = {16, 17, 32, 33}; //connect to the column pinouts of the keypad 25, 26, 27, 14
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// global variables

int numb = 0;
int blinkit = 0;
char myInput[4];
char myPasswort[] = "1956";
bool bol_OP=false;

//function prototype
bool Riddle_1();

// Setup
void setup() {
  Serial.begin(9600);

  // initialize the LCD
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Riddle 1");
  // Wait 5 seconds
  delay(5000);
  lcd.clear();
}

void loop() {
  while (!Riddle_1()and !bol_OP ){
    // not solved, look if anything received from Operator team
    };  
    lcd.clear();
    lcd.print("Riddle solved");
    delay(2000);
    while(1)
    {
      // send to operator and receive from operator
      }
}
bool Riddle_1() {
  while (numb != PWNUMB) {
    char customKey = customKeypad.getKey();
    //lcd.clear();
    if (customKey) {
      myInput[numb] = customKey;
      //Serial.println(customKey);
      lcd.print(customKey);
      numb++;
      if (numb == PWNUMB ) {
        if (strcmp(myInput, myPasswort) == 0) {
          return true;
        }
        else {
          delay(1000);
          while (blinkit < 4) {
            lcd.noBacklight();
            delay(500);
            lcd.backlight();
            delay(500);
            blinkit++;
          }
          blinkit = 0;
          lcd.clear();
          numb = 0;
          return false;
        }
      }
    }
  }
}
