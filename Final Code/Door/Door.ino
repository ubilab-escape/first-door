//--------------Ubilab - Escape Room WS 2019---------------------
//-----------Group 4 - Both doors + Entry Puzzle-----------------
//Code for motor control of the sliding door (Labroom --> Server room)


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Libraries ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "WiFi_MQTT_Json_Motor.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <AccelStepper.h>

//For code debugging
//#define DEBUG

//**********************************IMPORTANT*********************************************************************************************************
//This should be either "DOOR1" or "DOOR2" for the respective door
#define DOOR1

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Variables ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//wifi, mqtt clients
WiFiClient wifiClient;
PubSubClient client(wifiClient);

//ESP 32 Pin Connections

//...for Door1
#ifdef DOOR1
#define enPin 25
#define dirPin 32
#define pulPin 33
#define interruptOpenPin 26
#define interruptClosePin 23
#endif

//...for Door2
#ifdef DOOR2
#define enPin 25
#define dirPin 32
#define pulPin 33
#define interruptOpenPin 18
#define interruptClosePin 27
#endif

AccelStepper door1(1, pulPin, dirPin); // Pin assignment for the library

//Moving parameters
const int acceleration = 1300;
const int accelerationBreak = 1500;
const int maxVelocity = 3000;
const int minVelocity = 300; //unter 300 steps/sec -->microstepping : Dann alle Steps mit Faktor multiplizieren
const int calVelocity = 700;
const int minVelSteps = 500;   //300
const int breakSteps = 0.5 * sq(minVelocity) / acceleration ;
const int moveOn = 300;
float actSpeed;
const int breakStepsInput = 0.5 * sq(maxVelocity) / accelerationBreak;

//Moving variables
int curPos;
int endPosition;
int maxSteps;

bool brakeOpen = false;
bool brakeClose = false;



//Interrupt variables
volatile int interruptFlagO = 0;
volatile int interruptFlagC = 0;

//variables for alarm light
#ifdef DOOR1
String light_topic = "powermeter/gyrophare1/switch";
#endif
#ifdef DOOR2
String light_topic = "powermeter/gyrophare2/switch";
#endif


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ SETUP ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void setup() {

  Serial.begin(115200);

  //Pin declaration
  pinMode(enPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(pulPin, OUTPUT);
  pinMode(interruptOpenPin, INPUT_PULLUP);
  pinMode(interruptClosePin, INPUT_PULLUP);

#ifdef DOOR1
  pinMode(14, OUTPUT);
  digitalWrite(14, HIGH); //Supply for LED on endstop board
  pinMode(27, OUTPUT);
  digitalWrite(27, LOW); //GND for endstop board
#endif

#ifdef DOOR2
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH); //Supply for LED on endstop board
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW); //GND for endstop board
#endif

  //Interrupt for door stop detection
  attachInterrupt(digitalPinToInterrupt(interruptOpenPin), interruptOpen, FALLING); //Interrupt at the end of door opening due to switch touching
  attachInterrupt(digitalPinToInterrupt(interruptClosePin), interruptClose, RISING); //Interrupt at the end of door closing due to switch touching

  setup_wifi();
  setupOTA();
  setup_mqtt();
  //start MQTT communication
  mqtt_loop();

  //after wifi stuff calibrate the door!
  //turn alarm light on
  client.publish(light_topic.c_str(), "on");
  calibration();

  //after calibration open the door that all rooms are open
  openDoor = true;
  openDoorFunc();
  openDoor = false;

  //turn alarm light off
  client.publish(light_topic.c_str(), "off");
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ LOOP ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void loop() {
  //reconnect to Wifi
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  // Arduino OTA
  ArduinoOTA.handle();

  //(re-) connect to mqtt server and check for incoming messages
  mqtt_loop();

  if (openDoor == true) {
    
    //activate alarm light
    client.publish(light_topic.c_str(), "on");
    openDoorFunc();
    openDoor = false;
    //turn alarm light off
    client.publish(light_topic.c_str(), "off");
    
  }
  else if (closeDoor == true) {
    //activate alarm light
    client.publish(light_topic.c_str(), "on");
    closeDoorFunc();
    closeDoor = false;
    //turn alarm light off
    client.publish(light_topic.c_str(), "off");
  }

}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++Functions++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//*******************************************************************************************************
//******************************************Interrupts****************************************************
//********************************************************************************************************
void interruptOpen()
{
  interruptFlagO++;
#ifdef DEBUG
  Serial.println("An interrupt (open) has occurred!");
  Serial.println("Counter:");
  Serial.println(interruptFlagO);
#endif
}

void interruptClose()
{
  interruptFlagC++;
#ifdef DEBUG
  Serial.println("An interrupt (close) has occurred!");
  Serial.println("Counter:");
  Serial.println(interruptFlagC);
#endif
}



//*******************************************************************************************************
//******************************************Calibration Door****************************************************
//********************************************************************************************************

void calibration()
{
  Serial.println("Calibration in progress");
  //delay(5000);
  digitalWrite(enPin, LOW); //Enable Driving Stage

  interruptFlagO = 0;

  door1.setMaxSpeed(calVelocity);
  door1.setAcceleration(acceleration);

  //firstly, drive to right (open) ONLY if the endstop is not activated (door open alread)
  if (digitalRead(interruptOpenPin) != LOW)
  {
    while (true)
    {
      door1.setSpeed(calVelocity);
      door1.runSpeed();
      if (interruptFlagO > 0)
      {
        break;
      }
    }
  }

  door1.setCurrentPosition(0);
  delay(1000);

  interruptFlagC = 0;
  //while(interruptFlagC == 0)

  while (true)
  {
    door1.setSpeed(-calVelocity);
    door1.runSpeed();
    if (interruptFlagC > 0)
    {
      break;
    }
  }
  maxSteps = abs(door1.currentPosition());
  door1.setCurrentPosition(0);
  delay(2000);
  //digitalWrite(enPin, HIGH); //Disable Driving Stage

  Serial.println("Calibration done!");
}


//*******************************************************************************************************
//******************************************Close Door****************************************************
//********************************************************************************************************
void closeDoorFunc() {
  //Only close door if endstop not activated
  if (!digitalRead(interruptClosePin)) {
    #ifdef DEBUG
    Serial.println("Door closes!");
    #endif

    digitalWrite(enPin, LOW); //Enable Driving Stage
    interruptFlagC = 0; //Reset InterruptCounter
    door1.setMaxSpeed(0); //Set all velocity variable in the library to zero (BUG)

    //Check current position if door is in main speed section
    if (curPos > minVelSteps) {
      MoveDoor();
      //if during main speed phase closing, mqtt message 'open door' was received, start breaking in closing direction
      if (closeDoor == false) {
        Brake("brakeClose");
        return; //after braking leave openDoorFunc
      }
    } //Door reached minVelSteps or stopped due to new input in mqtt_loop


    //check current position if door is in slow speed section
    if (curPos <= minVelSteps) {
      MoveDoorSlow();
    }
    #ifdef DEBUG
    Serial.println("Door closed!");
    #endif
  }
}
//*******************************************************************************************************
//******************************************Open Door****************************************************
//********************************************************************************************************
void openDoorFunc() {

  //Only open door if endstop not activated (different level than close endstop!)
  if (digitalRead(interruptOpenPin)) {
    #ifdef DEBUG
    Serial.println("Door opens!");
    #endif
    digitalWrite(enPin, LOW); //Enable Driving Stage
    interruptFlagO = 0; //Reset InterruptCounter
    door1.setMaxSpeed(0); //Set all velocity variable in the library to zero (BUG)

    //Check current position if door is in main speed section
    if (curPos < (maxSteps - minVelSteps)) {
      MoveDoor();
      //if during main speed section closing, mqtt message 'close door' was received, start breaking in opening direction
      if (openDoor == false) {
        Brake("brakeOpen");
        return; //after braking leave openDoorFunc
      }
    }//Door reached minVelSteps or stopped due to new input in mqtt_loop

    //check current position if door is in slow speed section
    if (curPos >= (maxSteps - minVelSteps)) {
      MoveDoorSlow();
    }
    #ifdef DEBUG
    Serial.println("Door open!");
    #endif
  }
}

//*******************************************************************************************************
//****************************************** Brake ****************************************************
//********************************************************************************************************

void Brake(String brakeDirection) {
  //Moving Parameters
  door1.setMaxSpeed(maxVelocity);
  door1.setAcceleration(accelerationBreak);

  if (brakeDirection == "brakeOpen") {
    endPosition = curPos + breakStepsInput;
    //avoid additional acceleration if calculated endposition is greater then max value
    if (endPosition > maxSteps) {
      endPosition = maxSteps;
    }

    door1.moveTo(endPosition);
    while (curPos < endPosition) {
      door1.run();
      curPos = door1.currentPosition();

      //Enstop touched. Safety, if calibration parameters are incorrect
      if (interruptFlagO > 0) {
        delay(1000); //to prevent door from moving freely with rest impuls
        digitalWrite(enPin, HIGH); //Disable Driving Stage
        break;
      }
    }
  }

  else if (brakeDirection == "brakeClose") {
    endPosition = curPos - breakStepsInput;
    //avoid additional acceleration if calculated endposition is greater then max value
    if (endPosition < 0) {
      endPosition = 0;
    }
    door1.moveTo(endPosition);
    while (curPos > endPosition) {
      door1.run();
      curPos = door1.currentPosition();

      //Enstop touched. Safety, if calibration parameters are incorrect
      if (interruptFlagC > 0) {
        //delay(1000); //to prevent door from moving freely with rest impuls
        //digitalWrite(enPin, HIGH); //Disable Driving Stage
        break;
      }
    }
  }
}

//*******************************************************************************************************
//****************************************** MoveDoor ****************************************************
//********************************************************************************************************

void MoveDoor() {

  //Moving Parameters
  door1.setMaxSpeed(maxVelocity);
  door1.setAcceleration(acceleration);

  if (openDoor == true) {
    door1.moveTo(maxSteps - minVelSteps + breakSteps);
    //Open Door up to position where continuing with constant speed
    while ((curPos < (maxSteps - minVelSteps)) && openDoor == true) {
      door1.run();
      curPos = door1.currentPosition();
      mqtt_loop();

      //Enstop touched. Safety, if calibration parameters are incorrect
      if (interruptFlagO > 0) {
        delay(1000); //to prevent door from moving freely with rest impuls
        digitalWrite(enPin, HIGH); //Disable Driving Stage
        return;
      }
    }
  }

  else if (closeDoor == true) {
    door1.moveTo(minVelSteps - breakSteps);
    //Close Door up to position where continuing with constant speed
    while (curPos > minVelSteps && closeDoor == true) {
      door1.run();
      curPos = door1.currentPosition();
      mqtt_loop();

      //Enstop touched. Safety, if calibration parameters are incorrect
      if (interruptFlagC > 0)
      {
      delay(1000);
      digitalWrite(enPin, HIGH); //Disable Driving Stage
      #ifdef DOOR1
      //send closed door message to disable plasma globe puzzle
      mqtt_publish("4/door/entrance", "message", "door_1_closed");
      //avoid reopening if plasma globes are released before they read the message
      delay(1000);
      mqtt_loop();
      if(openDoor == true) openDoor = false;
      #endif
      return;
      }
    }
  }
}

//*******************************************************************************************************
//****************************************** MoveDoorSlow ****************************************************
//********************************************************************************************************

void MoveDoorSlow() {
  //Moving Parameters
  door1.setMaxSpeed(minVelocity);
  door1.setAcceleration(acceleration);

  if (openDoor == true) {
    door1.moveTo(maxSteps + moveOn);
    while ((curPos < (maxSteps + moveOn)) && (openDoor == true)) {
      door1.run();
      curPos = door1.currentPosition();
      mqtt_loop();

      if (interruptFlagO > 0) {
        delay(1000);
        digitalWrite(enPin, HIGH); //Disable Driving Stage
        return;
      }
    }
  }

  else if (closeDoor == true) {
    door1.moveTo(-moveOn);
    while ((curPos > (-moveOn)) && closeDoor == true) {
      door1.run();
      curPos = door1.currentPosition();
      mqtt_loop();

      if (interruptFlagC > 0) {
        delay(1000);
        digitalWrite(enPin, HIGH); //Disable Driving Stage
        #ifdef DOOR1
        //send closed door message to disable plasma globe puzzle
        mqtt_publish("4/globes", "message", "door_1_closed");
        //avoid reopening if plasma globes are released before they read the message
        delay(1000);
        mqtt_loop();
        if(openDoor == true) openDoor = false;
        #endif
        return;
      }
    }
    if(closeDoor == true){
      //this section of the code is only reached if door moves below zero (happens if endstop is damaged)
      delay(1000);
      digitalWrite(enPin, HIGH); //Disable Driving Stage
      #ifdef DEBUG1
      //send closed door message to disable plasma globe puzzle
      mqtt_publish("4/door/entrance", "message", "door_1_closed");
      //avoid reopening if plasma globes are released before they read the message
      delay(1000);
      mqtt_loop();
      if(openDoor == true) openDoor = false;
      #endif
    }
  }
}
