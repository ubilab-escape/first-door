# first-door #
Both Doors &amp; First Door Puzzle

## Project Status ##
- [ ] first Puzzle
	* ESP32 software is in progress: Tests with keypad, lcd
	* first Hardware test completed
	* __next steps__: mqtt communication, room integration
- [x] door mechanics
- [ ] first door software (motor control, end switch, current sensor, lock)
	* motor control via ESP32 + end stop (switch) interrupt is in progress
	* software for current sensor ACS712 in progress
	* __next_steps__: determine velocity and acceleration setting (after mechanical implementation in escape room), current measurements, mqtt communication 
- [ ] second door software
	* see first door software
- [ ] plasma globe puzzle
	* software for current sensor ACS712 in progress
	* __next_steps__: electrical setup and current measurements
- [ ] Wifi/MQTT/Json communication
	* communication structure and flow developed with operator group 
	* first test with mosquitto server run on PC and ESP32 client was successful
	* __next_steps__: implement communication for each ESP32

## First Puzzle ##
### Stage ###
After the mission briefing, the participants are in the anteroom and need to get access to the labroom.

### Idea ###
* keypad with lcd display
* pin is encoded as morse code 
	* variant 1: Acoustic signal from MP3 player that the security guy forgot in the anteroom (potentially mixed into a song)
	* variant 2: Acoustic signal from implemented sound system (potentially mixed into a song)
	* variant 3: Optical signal (LEDs)
* optional: place additional hints/ puzzles in anteroom with alternative pins that could be tested???

### Material ###
* keypad
* lcd display
* mounting
* MP3 player

## First Door ##
### Stage ###
Door between anteroom and labroom
Door should open after solving the first puzzle and close after all participants entered the labroom. Finally, it should open again, after prototype was placed back, so that the participants can escape.

### Idea ###
* sliding door with toothbelt mechanism
* opened/closed automatically by step-motor
* electrical door lock
* electromechanical switch detects closed and opened position
* obstacle detection in closing area: 
	* variant 1: tracking of current draw (expected to increase significantly if an obstacle is present)
	* variant 2: ligth barrier (ultra sound) / power measurement of motor

### Material ###
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
* current sensor (ACS712)

## Plasma Globe Puzzle ##
### Stage ###
When the first door opens, participants enter the labroom. There, all lights are off and the plasma globes are activated. The participants must touch all plasma balls simultaneously so that the first door can close and the game continues.

### Idea ###
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

### Material ###
* ESP32
* plasma globes
* buck converter (24V-->5V)/ power plug
* current sensor (ACS712)

## Communication ##
The MQTT communication that was developed with the operator group can be found here:
* [Doors](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_door.svg "Doors")
* [First Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_entrance_door.svg "First Puzzle")
* [Plasma Globe Puzzle](https://github.com/ubilab-escape/operator/blob/master/doc/design/group_4_puzzle_globes.svg "Plasma Globe Puzzle")