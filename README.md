# Group 4:  First door #
Our group works on the automated doors in the escape room as well on the puzzle for the first door.

## Content
* [Part in the Escape Room](#1)

## Part in the Escape Room <a name="1"></a>
After the mission briefing the participants will enter the anterior room.
In the story this will be the room of the security even though the security will be absent for the whole game.

The participants have to solve a first puzzle to open the first door and to be able to move to the second, adjencted room which will be the computer lab room. After all players are situated in the second room, the first door should close.

There the players again have to solve various riddles and puzzle to get into the server room. The door between lab and serverroom will be implemented by us.

## Project Timeline ##
| Description of work task | Target date | Actual date|
|:--------------|:-------------|:--------------|
| Concept finding| 10. Nov 2019 |10. Nov 2019 |
| First part list| 12. Nov 2019 | 12. Nov 2019|
| Implementation Hardware door| 01. Dec 2019| 01. Dec 2019|
| Riddle Implementation| 31. Dec 2019| 31. Dec 2019| 
| Dead man switch| 10 Jan 2020 | 10. Jan 2020|
| Testing and Bug fixing| 01. Feb 2020 |  01. Feb 2020 |
| Presentation| 17. Feb 2020 | 17. Feb 2020 |



## First Puzzle ##
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
The used software is found under *first-door/Final Code/First_Puzzle_final/*.
//Ablaufdiagramm muss hier rein
### MQTT Communication ###
The needed MQTT commands are:
* "4/puzzle" -m "{\"method\":\"trigger\", \"state\": \"on\"}" for activating the puzzle
* "4/puzzle" -m "{\"method\":\"trigger\", \"state\": \"off\"}" for deactivating the puzzle


## First Door ##
### Stage ###
This is the door between anteroom and labroom. This door should open after solving the first puzzle and close after all participants entered the labroom. After the participants were able to solve all other riddles the door should open again to let the players escape.

### Concept and Idea ###
* sliding door with toothbelt mechanism
* opened/closed automatically by step-motor
* electromechanical switch detects closed and opened position

Not implemented possible improvements:
* electrical door lock
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

### Software ##
The corresponding software can be found under: *first-door/Final Code/Door_1/*


## Dead man switches - Plasma globes ##
### Stage ###
When the first door opens, participants enter the labroom. There, all lights are off and the plasma globes are activated. The participants must touch all plasma balls simultaneously so that the first door can close and the game continues.

### Concept ###
* assumption: plasma globes draw significantly more current when touched so they can be used as switch(___to be tested!___)
* several plasma globes spread across the room at the walls (not possible to touch more than one globe simultaneously)
* Each plasma globe is a standalone unit with own ESP32
* One "master" device, the other globes are slaves
* operator sends #participants, then each globe determines if it is activated or deactivated
* each slave globe sends status (touched/ not touched) to master globe
* only if all globes are touched simultaneously, door should close (otherwise, it should open again)
* master globe communicates with first door and triggers opening/closing of door
* as soon as door is closed, it publishes a message to deactivate all globes (puzzle solved)
* _alternative_: use push buttons

### Hardware ###
* ESP32
* plasma globes
* buck converter (24V-->5V)/ power plug
* current sensor (ACS712)

## Communication ##
The MQTT communication that was developed with the operator group can be found here:
* [Doors](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_door.svg "Doors")
* [First Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_entrance_door.svg "First Puzzle")
* [Plasma Globe Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_globes.svg "Plasma Globe Puzzle")
