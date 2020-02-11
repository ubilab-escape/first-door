# Group 4:  First door #
Our group works on the automated doors in the escape room as well on the puzzle for the first door.

## Content
* [Part in the Escape Room](#1)
* [Project Timeline](#2)
* [General Program Flow](#3)
* [Using the code](#7)
* [First Puzzle](#4)
* [Doors](#5)
* [Plasma Globes](#6)

## Part in the Escape Room <a name="1"></a>
After the mission briefing the participants will enter the anterior room.
In the story this will be the room of the security even though the security will be absent for the whole game.

The participants have to solve a first puzzle to open the first door and to be able to move to the second, adjencted room which will be the computer lab room. After all players are situated in the second room, the first door should close.

There the players again have to solve various riddles and puzzle to get into the server room. The door between lab and serverroom will be implemented by us.

## Project Timeline <a name="2"></a>
- [x] Concept finding for door mechanism, puzzles and dead man switches
- [x] Composition of a part list
- [x] Installing of the doors
- [x] Software development of the doors and electrical installation
- [x] Software development of the riddle and installation of its needed components
- [x] Software development of the MQTT communication
- [x] Testing of the door and riddle components over MQTT
- [x] Testing of the voltage/current course of the plasma globes
- [x] Software implementation of the plasma globes
- [x] 3D printing of fixations and riddle housing
- [x] Testing of the interaction of all components
- [x] Implementation and control over the LED stripes from the Environment Group
- [x] Last testing and bug fixing
* Final run and presentation

## General Program Flow <a name="3"></a>

## Using the Code <a name="7"></a>
### General
* WiFi- credentials are not included in the code. Before uploading via OTA, ensure that the correct WiFi credentials are set in the code!

### Door Code
* Ensure that the #define macro DOOR1 or DOOR2 is set according to which door you want to use the code for

### Plasma Globes
* When you change the mdns name via MQTT, this will also change the ID of the globe (gets written to EEPROM). Use the Format "GlobeX" as new name, e.g. "Globe0" for the Master Globe.
* Max. 4 globes can be used (IDs: 0,1,2,3)


## First Puzzle <a name="4"></a>
### Stage ###
After the mission briefing, the participants are in the anteroom and need to get access to the labroom.

### Concept and Idea ###
Story: The absent security man likes to listen to music on his MP3-Player while at work. This MP3 contains several songs as well as an **audio morse code**. Using the help of a hidden morse alphabet in the room the players will be able to solve the puzzle and obtain a code.
![Morse alphabet](https://www.cnc14.de/content/5-projekte/2-cnc14-projekt-die-morse-verbindung/1-pic.jpg "test")

The obtained code has to be inserted into a keypad with an LCD display.

### Hardware ###
* MP3 Player
* 3D printed Input interface consisting of 
	* LCD display
	* 4x4 matrix keypad
	* ESP32

### Software ###
[Code](https://github.com/ubilab-escape/first-door/tree/master/Final%20Code/First_Puzzle_final)

The following flow chart only represents the basic function of the code and does not show every detail!

![flow chart puzzle 1](https://github.com/ubilab-escape/first-door/blob/master/flow%20charts/First_Puzzle_flowchart.png)

## Doors <a name="5"></a>
### Stage ###
Door 1:
This is the door between anteroom and labroom. This door should open after solving the first puzzle and close after all participants entered the labroom. After the participants were able to solve all other riddles the door should open again to let the players escape.

Door 2:

### Concept and Idea ###
* sliding door with toothbelt mechanism
* opened/closed automatically by step-motor
* electromechanical switch detects closed and opened position

Not implemented possible improvements:
* electrical door lock (not implemented because.....)
* obstacle detection in closing area: 
	* variant 1: tracking of current draw (expected to increase significantly if an obstacle is present)
	* variant 2: ligth barrier (ultra sound) / power measurement of motor

### Hardware ###
* ESP32
* stepper motor + appropriate driver
* tooth belt
* toothbelt disk
* electrical lock
* end stop switches
* linear rail
* mounting door - linear track
* mounting door - tooth belt
* door material (wooden plates)
* U-profile duct at floor (wooden)

### Software
[Code](https://github.com/ubilab-escape/first-door/tree/master/Final%20Code/Door)

## Plasma globes <a name="6"></a>
### Stage ###
When the first door opens, participants enter the labroom. There, all lights are off and the plasma globes are activated. The participants must touch all plasma balls simultaneously so that the first door can close and the game continues.

### Concept and Idea ###
* plasma globes draw significantly more current when touched so they can be used as switch
* 4 plasma globes spread across the room at the walls (not possible to touch more than one globe simultaneously)
* Each plasma globe is a standalone unit with own "PowerMeter"
* One "master" device, the other globes are slaves
* operator sends #participants, then each globe determines if it is activated or deactivated (according to globe ID which can be set over MQTT)
* Slave globes:
	* check if globe is touched
	* send touched/ untouched message to master globe
* Master globes:
	* check if globe is touched
	* count number of touched slaves
	* trigger opening of door if all participating globes are touched simultaneously
	* trigger closing of door if not all participating globes are touched
	* activate environment LEDs according to number of touched slaves to provide feedback to the user
	* deactivates all globes as soon as door is closed and send puzzle solved message

### Hardware ###
* PowerMeter (Hardware Box containing ESP8266 and AC current sensor )
* 4 usb powered plasma globes

### Software ###
[Master Globe - Code](https://github.com/ubilab-escape/first-door/tree/master/Final%20Code/PowerMeter_plasma_master)

[Slave Globe - Code](https://github.com/ubilab-escape/first-door/tree/master/Final%20Code/PowerMeter_plasma_slaves)

The following flow chart only represents the basic function of the code and does not show every detail!
![flow chart puzzle 1](https://github.com/ubilab-escape/first-door/blob/master/flow%20charts/Master_globe_flowchart.png)


## MQTT Communication ##
The MQTT communication that was initially developed with the operator group can be found here (not every detail was implemented):
* [Doors](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_door.svg "Doors")
* [First Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_entrance_door.svg "First Puzzle")
* [Plasma Globe Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_globes.svg "Plasma Globe Puzzle")

For controlling the described escape room parts of our group, use the commands in the following document:
[MQTT commands](https://github.com/ubilab-escape/first-door/blob/master/MQTT_publish_messages.docx "MQTT commands")
