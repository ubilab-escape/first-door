//--------------Ubilab - Escape Room WS 2019---------------------
//-----------Group 4 - Both doors + Entry Puzzle-----------------
//Code for motor control of the sliding door (Labroom --> Server room)

#include "WiFi_MQTT_Json_Motor.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include <AccelStepper.h>

#define DEBUG
#define DOOR1

//*****************Variables*******************
//Define pin connections
#ifdef DOOR1
#define enPin 25
#define dirPin 32
#define pulPin 33
#define interruptOpenPin 26
#define interruptClosePin 23
#endif

#ifdef DOOR2
#define enPin 25
#define dirPin 32
#define pulPin 33
#define interruptOpenPin 18
#define interruptClosePin 27
#endif

//moving parameters
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

int curPos;
int endPosition;
int maxSteps;
int actPos;

bool newInput = false;

//Interrupt variables
volatile int interruptFlagO = 0;
volatile int interruptFlagC = 0;

AccelStepper door1(1, pulPin, dirPin);


//******************Setup function*********************************
void setup() {

  Serial.begin(115200);

  //Application setup
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
  digitalWrite(27, LOW);
  #endif

  #ifdef DOOR2
  pinMode(21,OUTPUT);
  digitalWrite(21,HIGH); //Supply for LED on endstop board
  pinMode(19,OUTPUT);
  digitalWrite(19,LOW);
  #endif

  //Interrupt for door1 stop detection
  attachInterrupt(digitalPinToInterrupt(interruptOpenPin), interruptOpen, FALLING); //Interrupt at the end of door opening due to switch touching
  attachInterrupt(digitalPinToInterrupt(interruptClosePin), interruptClose, RISING); //Interrupt at the end of door closing due to switch touching

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin(ssid, password);
  delay(1000);
  WiFi.begin(ssid, password);
  long timer = millis();
  while (millis() - timer < 10000) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(5000);
    Serial.print(".");
    WiFi.begin(ssid, password);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(100);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  #ifdef DOOR1
  ArduinoOTA.setHostname("Door1");
  #endif
  #ifdef  DOOR2
  ArduinoOTA.setHostname("Door2");
  #endif

  // No authentication by default
  ArduinoOTA.setPassword("group4");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]()
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]()
  {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error)
  {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Communication setup
  //setup_wifi();
  setup_mqtt();
  //setupOTA();
  //initMDNS();
  //mdnsUpdate = millis();

  //after wifi stuff calibrate the door!
  calibration();
}


//******************************Loop function****************************
void loop() {
  //reconnect to Wifi
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  // Arduino OTA
  ArduinoOTA.handle();


  //  // Re-advertise service
  //  if ((long)(millis() - mdnsUpdate) >= 0) {
  //    MDNS.addService("elec", "tcp", 54322);
  //    mdnsUpdate += 100000;
  //  }

  //(re-) connect to mqtt server and check for incoming messages
  mqtt_loop();

  if (openDoor == true) {

    openDoorFunc();
    openDoor = false;
  }
  else if (closeDoor == true) {

    closeDoorFunc();
    closeDoor = false;
  }

}

//********************Functions*****************************
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





void calibration()
{
  Serial.println("Calibration in progress");
  //delay(5000);
  digitalWrite(enPin, LOW); //Enable Driving Stage

  interruptFlagO = 0;

  door1.setMaxSpeed(calVelocity);
  door1.setAcceleration(acceleration);

  //firstly, drive to right (open) ONLY if the endstop is not activated (door open alread)
  if(digitalRead(interruptOpenPin) != LOW)
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

//**********************************Close Door*****************************************
void closeDoorFunc()
{
  //Only close door if endstop not activated
  if(!digitalRead(interruptClosePin)){
    #ifdef DEBUG
    Serial.println("Door closes!");
    #endif  
  
    digitalWrite(enPin, LOW); //Enable Driving Stage  
    interruptFlagC = 0; //Reset InterruptCounter
    door1.setMaxSpeed(0); //Set all velocity variable in the library to zero (BUG)

    //Check current position if door is in main speed section
    if(curPos > minVelSteps)
     {
     //Moving Parameters
     door1.setMaxSpeed(maxVelocity);
     door1.setAcceleration(acceleration);
     door1.moveTo(minVelSteps-breakSteps);
  
      //Close Door up to position where continuing with constant speed
      while(curPos > minVelSteps && closeDoor == true)
        {
        door1.run();
        curPos = door1.currentPosition();
        mqtt_loop();

        //Enstop touched. Safety, if calibration parameters are incorrect
        if(interruptFlagC > 0)
          {
            //delay(1000); //to prevent door from moving freely with rest impuls
            //digitalWrite(enPin, HIGH); //Disable Driving Stage
            return;
          }
        }
  
      //if during main speed phase closing, mqtt message 'open door' was received, start breaking in closing direction
      if(closeDoor == false)
        {
          door1.setAcceleration(accelerationBreak);
          endPosition = curPos - breakStepsInput;
          //avoid additional acceleration if calculated endposition is negative
          if(endPosition < 0){
            endPosition = 0;
          }
          door1.moveTo(endPosition);
          
    
          while(curPos > endPosition)
            {
              door1.run();
              curPos = door1.currentPosition();

              //Enstop touched. Safety, if calibration parameters are incorrect
              if(interruptFlagC > 0)
                {
                  //delay(1000); //to prevent door from moving freely with rest impuls
                  //digitalWrite(enPin, HIGH); //Disable Driving Stage
                  return;
                }
            }    
        return; //after breaking leave closeDoorFunc   
        }
     
      } //Door reached minVelSteps or stopped due to new input in mqtt_loop

    //check current position if door is in slow speed section
    if(curPos <= minVelSteps)
     {
     //Moving Parameters
     door1.setMaxSpeed(minVelocity);
     door1.setAcceleration(acceleration);
     door1.moveTo(-moveOn);

      //
      while((curPos > (-moveOn)) && closeDoor == true)
        {
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
      
        if(interruptFlagC>0)
         {
         delay(1000);
         digitalWrite(enPin, HIGH); //Disable Driving Stage
         //send closed door message to disable plasma globe puzzle
         mqtt_publish("4/Globes", "message", "door_1_closed"); 
         return;
         }
        }
      
      }
  
      //this section of the code is only reached if door moves below zero (happens if endstop is damaged)
      delay(1000);
      digitalWrite(enPin, HIGH); //Disable Driving Stage
  
      //send closed door message to disable plasma globe puzzle
      mqtt_publish("4/Door/Entrance", "message", "door_1_closed");
      
      #ifdef DEBUG
      Serial.println("Door closed!");
      #endif 
   
    }
}

//******************************************Open Door****************************************************+
void openDoorFunc()
{
  //Only open door if endstop not activated (different level than close endstop!)
  if(digitalRead(interruptOpenPin)){
    
    #ifdef DEBUG
    Serial.println("Door opens!");
    #endif
    digitalWrite(enPin, LOW); //Enable Driving Stage  
    interruptFlagO = 0; //Reset InterruptCounter
    door1.setMaxSpeed(0); //Set all velocity variable in the library to zero (BUG)

    //Check current position if door is in main speed section
    if (curPos < (maxSteps - minVelSteps))
    {
      //Moving Parameters
      door1.setMaxSpeed(maxVelocity);
      door1.setAcceleration(acceleration);
      door1.moveTo(maxSteps - minVelSteps + breakSteps);

      //Open Door up to position where continuing with constant speed
      while ((curPos < (maxSteps - minVelSteps)) && openDoor == true)
      {
        door1.run();
        curPos = door1.currentPosition();
        mqtt_loop();
  
        //Enstop touched. Safety, if calibration parameters are incorrect
        if(interruptFlagO > 0)
          {
            delay(1000); //to prevent door from moving freely with rest impuls
            digitalWrite(enPin, HIGH); //Disable Driving Stage
            return;
          }
      }

      //if during main speed phase closing, mqtt message 'close door' was received, start breaking in opening direction
      if (openDoor == false) //Bremsvorgang - BEGINN
      {
        door1.setAcceleration(accelerationBreak);
        endPosition = curPos + breakStepsInput;
        //avoid additional acceleration if calculated endposition is greater the max value
        if(endPosition > maxSteps){
            endPosition = maxSteps;
        }
        door1.moveTo(endPosition);
  
        while (curPos < endPosition)
        {
          door1.run();
          curPos = door1.currentPosition();
          
          //Enstop touched. Safety, if calibration parameters are incorrect
          if(interruptFlagO > 0)
            {
              delay(1000); //to prevent door from moving freely with rest impuls
              digitalWrite(enPin, HIGH); //Disable Driving Stage
              return;
            }
        }    
        return; //after breaking leave closeDoorFunc 

      }//Bremsvorgang ENDE
    }//Door reached minVelSteps or stopped due to new input in mqtt_loop

    //check current position if door is in slow speed section
    if (curPos >= (maxSteps - minVelSteps))
    {
      //Moving Parameters
      door1.setMaxSpeed(minVelocity);
      door1.setAcceleration(acceleration);
      door1.moveTo(maxSteps + moveOn);
  
      while ((curPos < (maxSteps + moveOn)) && (openDoor == true))
      {
        door1.run();
        curPos = door1.currentPosition();
        mqtt_loop();
        
        if(interruptFlagO>0)
         {
           delay(1000);
           digitalWrite(enPin, HIGH); //Disable Driving Stage 
           return;
         }
      }
      
    }
    
    //this section of the code is only reached if door moves below max point (happens if endstop is damaged)
    delay(1000);
    digitalWrite(enPin, HIGH); //Disable Driving Stage
      
    #ifdef DEBUG
    Serial.println("Door open!");
    #endif   
  }
}
