void setup() {
  pinMode(R0_0, OUTPUT); // conveyor relay 1
  pinMode(R0_1, OUTPUT); // conveyor relay 2
  pinMode(R0_2, OUTPUT); // Red light
  pinMode(R0_3, OUTPUT); // Green light

  pinMode(I0_5, INPUT); // 3 position toggle switch reverse
  pinMode(I0_6, INPUT); // 3 position toggle switch forward
  pinMode(I0_7, INPUT); // e-stop button
  pinMode(I0_8, INPUT); // Start sensor
  pinMode(I0_9, INPUT); // End sensor

  #define red_light R0_2
  #define green_light R0_3

  #define switch_reverse I0_5
  #define switch_forward I0_6
  #define e_stop I0_7
  #define reflex_start I0_8
  #define reflex_end I0_9

}

void loop() {
// routing to different functions based off of switch and estop state
if (digitalRead(e_stop) == 1) {
conveyor_stop();
redblink();
} 
else if (digitalRead(switch_forward) == 1) {
switch_forward_func();
}
else if (digitalRead(switch_reverse) == 1) {
switch_reverse_func();
} else {
conveyor_stop();
}

// controls the red light based off of the lack of the estop being pressed and any one of the sensors being activated
if ((digitalRead(reflex_end) == 1 || digitalRead(reflex_start) == 1) && digitalRead(e_stop) == 0) { 
digitalWrite(red_light, 1);
} else if (digitalRead(e_stop) == 0) {
digitalWrite(red_light, 0);
}

}

// conditional functions for different switch states, inside of which it is dependent on the state of the reflex sensors
void switch_forward_func(){
  if (digitalRead(reflex_start) == 1 && digitalRead(reflex_end) == 0) {
  conveyor_fwd();
  } else if (digitalRead(reflex_end) == 1) {
  conveyor_stop();
  } else {
  conveyor_fwd();
  }
}
void switch_reverse_func(){
  if (digitalRead(reflex_end) == 1 && digitalRead(reflex_start) == 0) {
  conveyor_rev();
  } else if (digitalRead(reflex_start) == 1) {
  conveyor_stop();
  } else {
  conveyor_rev();
  }
}

// conveyor and green light control
void conveyor_fwd(){
  // motor forward
  digitalWrite(R0_0, 0);
  digitalWrite(R0_1, 1);
  // green light on
  digitalWrite(R0_3, 1);
}

void conveyor_rev(){
  // motor forward
  digitalWrite(R0_0, 1);
  digitalWrite(R0_1, 0);
  // green light on
  digitalWrite(R0_3, 1);
}

void conveyor_stop(){
  // motor forward
  digitalWrite(R0_0, 0);
  digitalWrite(R0_1, 0);
  // green light on
  digitalWrite(R0_3, 0);
}

// blinking red light control
unsigned long previousMillis = 0;        // stores last time Led blinked
long interval = 100;           // interval at which to blink (milliseconds)
void redblink(){
if (millis() - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = millis();
    digitalWrite(R0_2, !digitalRead(R0_2)); //change led state
  }
}
