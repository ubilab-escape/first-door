//--------------Ubilab - Escape Room WS 2019---------------------
//-----------Group 4 - Both doors + Entry Puzzle-----------------
//Code for motor control of the sliding door (Anteroom --> Labroom) 

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
//*****************Variables*******************
//Define pin connections
#define enPin 25
#define dirPin 32
#define pulPin 33
#define interruptOpenPin 26
#define interruptClosePin 23

//moving parameters
const int acceleration = 1300;
const int accelerationBreak = 2000;
const int maxVelocity = 3000;
const int minVelocity = 300; //unter 300 steps/sec -->microstepping : Dann alle Steps mit Faktor multiplizieren
const int calVelocity = 700;
const int minVelSteps = 300; 
const int breakSteps = 0.5*sq(minVelocity)/acceleration ;
const int moveOn = 300;
float actSpeed;
const int breakStepsInput=0.5*sq(maxVelocity)/accelerationBreak;

int curPos;
int endPosition;
int maxSteps;
int actPos;

bool newInput = false;

//Interrupt variables
volatile int interruptFlagO = 0;
volatile int interruptFlagC = 0;

AccelStepper door1(1,pulPin, dirPin);

const char* ssid = "";
const char* password = "";

//******************Setup function*********************************
void setup() {
  
  Serial.begin(115200);

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("Door1");

  // No authentication by default
  //ArduinoOTA.setPassword("");

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

  //Application setup
  //Pin declaration
  pinMode(enPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(pulPin, OUTPUT);
  pinMode(interruptOpenPin, INPUT_PULLUP);
  pinMode(interruptClosePin, INPUT_PULLUP);
  pinMode(13,OUTPUT);
  digitalWrite(13,HIGH); //Supply for LED on endstop board
  pinMode(27,OUTPUT);
  digitalWrite(27,LOW);

  //Interrupt for door stop detection
  attachInterrupt(digitalPinToInterrupt(interruptOpenPin),interruptOpen,FALLING); //Interrupt at the end of door opening due to switch touching
  attachInterrupt(digitalPinToInterrupt(interruptClosePin),interruptClose,RISING); //Interrupt at the end of door closing due to switch touching

  calibration();


  

}

//******************************Loop function****************************
void loop(){
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

  if(openDoor == true){
    
    openDoorFunc();
    openDoor = false;
  }
  else if (closeDoor == true){

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
    delay(5000);     
    digitalWrite(enPin, LOW); //Enable Driving Stage

    interruptFlagO = 0;

    door1.setMaxSpeed(calVelocity);
    door1.setAcceleration(acceleration);
    
    while(true)
    {     
      door1.setSpeed(calVelocity);
      door1.runSpeed();  
      if(interruptFlagO>0)
      {
        break;
        }
    }
      
    door1.setCurrentPosition(0);
    delay(1000);
    
    interruptFlagC = 0;
    //while(interruptFlagC == 0)
    
    while(true)
    {
      door1.setSpeed(-calVelocity);
      door1.runSpeed();
      if(interruptFlagC>0)
      {
        break;
        }
    }
    maxSteps=abs(door1.currentPosition());
    door1.setCurrentPosition(0); 
    delay(2000);
    digitalWrite(enPin, HIGH); //Disable Driving Stage
    
    Serial.println("Calibration done!");
    }


void closeDoorFunc()
  {
    interruptFlagC = 0;
    Serial.println("Door closes!");
    digitalWrite(enPin, LOW); //Enable Driving Stage
    door1.setAcceleration(acceleration);
    door1.setMaxSpeed(0);  
    curPos = door1.currentPosition();        

    if(curPos > minVelSteps)
    {
      endPosition = minVelSteps-breakSteps;
      door1.moveTo(endPosition);
      door1.setMaxSpeed(maxVelocity);
      
      while(curPos>minVelSteps && closeDoor == true)
        { 
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
        }

      endPosition = -moveOn;
      door1.moveTo(endPosition);
      door1.setMaxSpeed(minVelocity);
      
      while(((curPos+moveOn)>0) && closeDoor == true)
      {
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
        if(interruptFlagC>0)
          {
          break;
          }
      }
    }
      
      
    else if(curPos <= minVelSteps)
    {
      endPosition = -moveOn;
      door1.moveTo(endPosition);
      door1.setMaxSpeed(minVelocity);
      
      while(((curPos+moveOn)>0) && closeDoor == true)
      { 
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
        if(interruptFlagC>0)
          {
          break;
          }
      }
    }
    delay(1000);
    digitalWrite(enPin, HIGH); //Disable Driving Stage
    Serial.println("Door closed!");    
  }


void openDoorFunc()
  {
    interruptFlagO = 0;
    Serial.println("Door opens!");
    digitalWrite(enPin, LOW); //Enable Driving Stage
    door1.setSpeed(0);
    door1.setAcceleration(acceleration);
    door1.setMaxSpeed(0);  
    curPos = door1.currentPosition();        

    if(curPos<(maxSteps-minVelSteps))
    {
      endPosition = (maxSteps-minVelSteps+breakSteps);
      door1.moveTo(endPosition);
      door1.setMaxSpeed(maxVelocity);
      
      while((curPos<(maxSteps-minVelSteps)) && openDoor == true)
        { 
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();  
     
        }

      endPosition = (maxSteps+moveOn);
      door1.moveTo(endPosition);
      door1.setMaxSpeed(minVelocity);

      while((curPos<(maxSteps+moveOn)) && (openDoor == true))
        {
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
        
        if(interruptFlagO>0)
          {
          break;
          } 
          
        }

    
    }
      
      
    else if(curPos>=(maxSteps-minVelSteps))
    {
      endPosition = (maxSteps+moveOn);
      door1.moveTo(endPosition);
      door1.setMaxSpeed(minVelocity);
      
      while((curPos<(maxSteps+moveOn)) && (openDoor == true))
      { 
        door1.run();
        curPos=door1.currentPosition();
        mqtt_loop();
        if(interruptFlagO>0)
          {
          break;
          }
      }

    }
    delay(1000);
    digitalWrite(enPin, HIGH); //Disable Driving Stage
    Serial.println("Door opened!");      
   }
