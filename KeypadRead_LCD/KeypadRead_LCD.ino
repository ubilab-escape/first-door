/*
 * Description: This Project implements the functions of reading out the Keypad and showing the number on the LCD Display
 * Demands: 4 digit pin code, after wrong input whole number is blinking for 3 times then deleted
 * Materials: LCD display - I2C Schnittstelle , Keypad- 4x4 Matrix Keypad
 * Author: Sarah Eisenkolb
 * Date: 19.11.2019
 */

#include <Keypad.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
//I2C pins declaration
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 4, 3, 2}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad myKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

void RiddleSolved();

void setup() {
  Serial.begin(9600);
  lcd.begin();//Initializing display
  lcd.backlight();//To Power ON the back light
//lcd.backlight();// To Power OFF the back light

}
int numInput=0;
char myInput [4];
char myPassword={1234};

void loop() {
  char myKey = myKeypad.getKey();
  if(myKey){
    if (numInput <=3)
    {
      myInput[numInput]=myKey;
      lcd.clear();
      lcd.setCursor(0,0); //Defining positon to write from first row,first column.
      lcd.print(myInput);
      numInput++;

      if(numInput ==3){
        //check if input equals password
         if(myInput==myPassword){
          RiddleSolved();
         }
         else {
          for (int i=1; i<3; i++){
            lcd.clear();
            delay (500);  // in ms
            lcd.setCursor(0,0);
            lcd.print(myInput);
          }
          numInput=0;
          //myInput={0000};
         }
      }
      
    }
  } 
}

void RiddleSolved(){
  // Write over Wifi to operator room
  // Call mechanical door lock & make sure it opened
  // call motor for sliding door
}
