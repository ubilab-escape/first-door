# first-door #
Both Doors &amp; First Door Puzzle

## Project Status ##
- [ ] First Puzzle
	* ESP32 software is in progress: Tests with keypad, lcd
	* first Hardware test completed
- [ ] Door mechanics
- [ ] First door software (motor control, end switch, current sensor, lock)
	* motor control via ESP32 + end stop (switch) interrupt is in progress
- [ ] Second door software
- [ ] Plasma globe puzzle
- [ ] Wifi/MQTT/Json communication
	* first tests with mosquitto server run on PC and ESP32 client was successful
- [ ]

## First Puzzle ##
### Stage ###
After the mission briefing, the participants are in the anteroom and need to get access to the labroom.

### Idea ###
* keypad with lcd display
* pin is encoded as morse code 
	* Variant 1: Acoustic signal from MP3 player that the security guy forgot in the anteroom (potentially mixed into a song)
	* Variant 2: Acoustic signal from implemented sound system (potentially mixed into a song)
	* Variant 3: Optical signal (LEDs)
* optional: place additional hints/ puzzles in anteroom with alternative pins that should be tested???



	
	

Door implementation:
1) Sliding Door
	- toothbelt mechanism
	- opened automatically by step-motor
	- electrical door lock (fail-safe) to prevent participants from enforcing opening of the door
	- electromechanical switch detects closing
	- obstacle detection in closing area: ligth barrier (ultra sound) / power measurement of motor
	- emergency solution?
	- standalone version necessary?
	
2) Standard door
	- use the built-in door (mechanically self-closing door?)
	- electrical lock
	- electromechanical switch detects closing

Puzzle implementation:
	- Enter password into keypad
	- logical/ mathematical riddle to find keypad
	- find key on shredded paper

Dead-man-switch implementation:
1) Plasma bowl:
	- detect current change when touched (--> current measurement needed)
2) Buttons
	