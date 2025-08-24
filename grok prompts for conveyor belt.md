i have the industrial shields 10 I/Os relay board with an esp32 that i am using to control a conveyor belt, and is controlled by inputs and outputs. 

i fed the following prompt to chat gpt iteratively and i ended up with the attached file. I have the controls i need in real life all wired, but i want to add a web server component to it that has the ability to monitor the state of the inputs and the outputs, control the outputs, have it all organized in a table. the esp should start in the access point mode, where it will locally host a web server that someone can connect to and input the credentials of the wifi that they want the esp to connect to. it should then connect while still hosting the access point, and once connected tell the user on the access point web server what the ip address of the esp is on the specified network. the user will then disconnect from the esp ap and connect to the network and visit the ip address that the esp specified previously, and will be able to control the esp. it should host a web page using the asynchronous web socket protocol and allow the user to control outputs and monitor inputs and outputs from a table.

i have wired a switch to pin I0_4, and i would like it to switch between a mode where it is controlled and monitored in person, and monitored online (as it is currently programmed), and a mode where the switch that controls direction stops responding, but the estop continues working, and i can monitor the states in person, and monitor the states online, and also control the motor and the lights that portray the movement online. make the code for this, but do not inhibit the functionality of the physical mode. 

it should be accessible from the same site that can be used to monitor the state of everything.







take out the part of the code that allows you to control the red light from the control station, and add a toggle switch to the control page that puts it into object detect mode that will wait until there is an object detected at one end of the conveyor to start the motor in order to take it to the other end at which point it stops, and add another toggle that will decide in which direction it is going to take the object in object detection mode (fwd or rev), and add a toggle that will put it into a mode that will alternate the object between the two ends of the conveyor by detecting when the sensor is activated (reading a 1) on specified pin


bring back the three buttons in line that control the direction of the conveyor regardless of the state of the sensors, and make it so that alternate mode automatically starts at either end taking it to the other end of the conveyor, and when it gets to the other end, it will automatically switch the direction of the conveyor in order to take it back to where it started, and it should repeat that indefinitely until alternator mode is switched off. forward mode takes an object from the start sensor to the end sensor, and reverse mode takes an object from the end sensor to the start sensor. 


in alternating mode, it is going in the wrong direction, and not turning back when it gets to the other end