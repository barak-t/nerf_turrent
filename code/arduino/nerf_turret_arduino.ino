#include <Servo.h>

// #define DEBUG

#define JOYSTICK true
#define JOYSTICK_PIN_PAN A7
#define JOYSTICK_PIN_TILT A6
#define JOYSTICK_PIN_BTN 3

#define SERVO_PIN_RECOIL 11
#define SERVO_PIN_PAN 10
#define SERVO_PIN_TILT 9
#define MOTOR_PIN 6

#define SERIAL_BAUDRATE 115200

#define BUFFER_INDEX_PAN 0
#define BUFFER_INDEX_TILT 1
#define BUFFER_INDEX_MOTOR 2
#define BUFFER_INDEX_SHOOT 3


//----- Declare servos and variables
Servo recoil_servo;
Servo pan_servo;
Servo tilt_servo;

const byte pan_limit_min = 0;
const byte pan_limit_max = 180;
const byte tilt_limit_min = 90;
const byte tilt_limit_max = 180;
const byte recoil_rest = 180;     // Angle of the servo when at rest
const byte recoil_pushed = 90;  // Angle the servo need to reach to push the dart

byte pan = 90; // start pan value
byte tilt = tilt_limit_max; // start tilt value
byte last_serial_pan = 0; // last pan and tilt values got from serial con
byte last_serial_tilt = 0;
bool btn_pushed = false;

//----- Variables related to serial data handling
const byte startMarker = 255;
const byte endMarker = 254;
const byte buffSize = 30;
byte inputBuffer[buffSize];
byte bytesRecvd = 0;
boolean data_received = false;

//----- Variable related to motor timing and firing
bool is_firing =  false;
bool can_fire =  false;
bool motors_on = false;

const long motor_starts_time = 1000;
const long firing_time = 300;
unsigned long firing_start_time = 0;
unsigned long firing_current_time = 0;
unsigned long motor_started_time = 0;

const long recoil_time = 2 * firing_time;
unsigned long recoil_start_time = 0;
unsigned long recoil_current_time = 0;

const int speed = 10;
unsigned long joystick_tilt_last_time = 0;
unsigned long joystick_pan_last_time = 0;


void setup()
{
  if(JOYSTICK){
    pinMode(JOYSTICK_PIN_BTN, INPUT);
    digitalWrite(JOYSTICK_PIN_BTN, HIGH); 
    pinMode(2, OUTPUT); // config free pin for use as HIGH for wiring the joystick 5V.
    digitalWrite(2, HIGH);
  }
 
  //-----define motor pin mode
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  //-----attaches servo to pins
  recoil_servo.attach(SERVO_PIN_RECOIL);
  pan_servo.attach(SERVO_PIN_PAN);
  tilt_servo.attach(SERVO_PIN_TILT);

  //-----starting sequence
  recoil_servo.write(recoil_rest);
  move_servo();

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(1000);
  digitalWrite(13, HIGH);

  Serial.begin(SERIAL_BAUDRATE);  // begin serial communication
}


/*
 * main loop function
 */
void loop()
{
  read_data_from_serial();
  
  if (data_received || JOYSTICK) {
    update_values();
    move_servo();
    set_recoil();
    set_motor();
  }
  fire();
}


/*
 * read data from serial funtion
 * expected structure of data [start byte, pan amount, tilt amount, motor on, firing button pressed, end byte]
 * start byte = 255
 * pan amount = byte between 0 and 253
 * tilt amount = byte between 0 and 253
 * motor on = 0 for off - 1 on
 * firing button pressed = 0 for not pressed - 1 for pressed
 * end byte = 254
 */
void read_data_from_serial() {

  if (Serial.available()) {  // If data available in serial
    byte data_byte = Serial.read();   //read the next character available

    if (data_byte == startMarker) {     // look for start byte, if found:
      bytesRecvd = 0;                   // reset byte received to 0 (to start populating inputBuffer from start)
      data_received = false;
    }
    else if (data_byte == endMarker) {  // look for end byte, if found:
      data_received = true;             // set data_received to true so the data can be used
    }

    else {                            // add received bytes to buffer
      inputBuffer[bytesRecvd] = data_byte;  //add byte to input buffer
      bytesRecvd++;                         // increment byte received
      if (bytesRecvd == buffSize) {    // just a security in case the inputBuffer fills up (shouldn't happen)
        bytesRecvd = buffSize - 1;     // if bytesReceived > buffer size set bytesReceived smaller than buffer size
      }
    }
  }
}


/*
 * update values functions
 * read the values of pan and tilt form the joystic and the serial buffer
 * and update the variables
 */
void update_values(){

  if(JOYSTICK){
    btn_pushed = !digitalRead(JOYSTICK_PIN_BTN);
    int pan_read = analogRead(JOYSTICK_PIN_PAN);
    int tilt_read = analogRead(JOYSTICK_PIN_TILT);
    #ifdef DEBUG
      Serial.println((String)btn_pushed + "," + pan_read + "," + tilt_read);
    #endif

    unsigned long now = millis();
    if(pan_read < 490 && pan < pan_limit_max){ // move left
      if(now - joystick_pan_last_time > speed){
        pan++;
        joystick_pan_last_time = now;
      }
    }
    else if(pan_read > 530 && pan > pan_limit_min){ // move right
      if(now - joystick_pan_last_time > speed){
        pan--;
        joystick_pan_last_time = now;
      }
    }
    if(tilt_read < 490 && tilt < tilt_limit_max){ // move down
      if(now - joystick_tilt_last_time > speed){
        tilt++;
        joystick_tilt_last_time = now;
      }
    }
    else if(tilt_read > 530 && tilt > tilt_limit_min){ // move up
      if(now - joystick_tilt_last_time > speed){
        tilt--;
        joystick_tilt_last_time = now;
      }
    }
    #ifdef DEBUG
      Serial.println((String)"PAN:" + pan + ", TILT: " + tilt);
    #endif
  }

  // Check if got new values for pan and tilt from serial buffer
  if(data_received && ( last_serial_pan != inputBuffer[BUFFER_INDEX_PAN] || 
          last_serial_tilt != inputBuffer[BUFFER_INDEX_TILT])){
    last_serial_pan = inputBuffer[BUFFER_INDEX_PAN];
    last_serial_tilt = inputBuffer[BUFFER_INDEX_TILT];
    pan = last_serial_pan;
    tilt = last_serial_tilt;
    pan = map(pan, 0, 180, pan_limit_max, pan_limit_min); // convert inputbuffer value to servo position value
    tilt = map(tilt, 0 , 180, tilt_limit_min, tilt_limit_max);     // convert inputbuffer value to servo position value
  }
}


/**
 * move servo function
 * move the pan and tilt servos acording the globle variables
 */
void move_servo() {
  pan_servo.write(pan); //set pan servo position
  tilt_servo.write(tilt); //set pan servo position
}


/**
 * set recoil function
 * 
 */
void set_recoil() {

  if (inputBuffer[BUFFER_INDEX_SHOOT] == 1 || btn_pushed) {   //if fire button pressed
    if (!is_firing) { //and not already firing or recoiling
      can_fire = true;              //set can fire to true (see effect in void fire())
    }
  }
  else {                  // if fire button not pressed
    can_fire = false;     //set can fire to false (see effect in void fire())
  }
}

/**
 * set motor function
 * start and stop motors using TIP120 transisitor .
 */
void set_motor() {

  if (!motors_on && (inputBuffer[BUFFER_INDEX_MOTOR] == 1 || btn_pushed)) {

    digitalWrite(MOTOR_PIN, HIGH); // turn motor ON
    motor_started_time = millis();
    motors_on = true;
  }
  else if (!is_firing && motors_on && (inputBuffer[BUFFER_INDEX_MOTOR] == 0 && !btn_pushed)) {                        //if screen not touched
    digitalWrite(MOTOR_PIN, LOW); // turn motor OFF
    motors_on = false;
  }
}

/**
 * fire function
 * if motor byte on, turn motor on and check for time it has been on
 */
void fire() {

  if (can_fire && !is_firing && motors_on) {
    if(millis() - motor_started_time > motor_starts_time){
      is_firing = true;
      firing_start_time = millis();    
      recoil_start_time = millis();
    }
  }

  firing_current_time = millis();
  recoil_current_time = millis();
  
  if(is_firing){
    if (firing_current_time - firing_start_time < firing_time) {
       recoil_servo.write(recoil_pushed);
    }
    else if (recoil_current_time - recoil_start_time < recoil_time) {
      recoil_servo.write(recoil_rest);
    }
    else if (recoil_current_time - recoil_start_time > recoil_time) {
      is_firing = false;
    }
  }

}
