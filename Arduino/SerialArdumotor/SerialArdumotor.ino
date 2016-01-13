/* Read Ardumotor speed from serial port 
 * Jason Antico
 */

int pwm_a = 3;   //PWM control for motor outputs 1 and 2 is on digital pin 3
int pwm_b = 11;  //PWM control for motor outputs 3 and 4 is on digital pin 11
int dir_a = 12;  //direction control for motor outputs 1 and 2 is on digital pin 12
int dir_b = 13;  //direction control for motor outputs 3 and 4 is on digital pin 13
int val = 0;     //value for fade
bool stopChecking = false;
int lastSpeed = 0;

void setup() {
   // Choose a baud rate and configuration. 9600 
   // Default is 8-bit with No parity and 1 stop bit
   Serial.begin(2400, SERIAL_8N1);

  pinMode(pwm_a, OUTPUT);  //Set control pins to be outputs
  pinMode(pwm_b, OUTPUT);
  pinMode(dir_a, OUTPUT);
  pinMode(dir_b, OUTPUT);
  
  analogWrite(pwm_a, 100);  //set both motors to run at (100/255 = 39)% duty cycle (slow)
  analogWrite(pwm_b, 100);
}

void loop() {
   byte charIn;
   int desiredSpeed = 0;
   if (Serial.available()) {
     desiredSpeed = Serial.parseInt();
     if (desiredSpeed == 666) {
        stopChecking = !stopChecking;
        Serial.print("Received stop signal");
        Serial.println();
     }
     
     if(!stopChecking){          // A byte has been received
  //      charIn = Serial.read() - '0';       // Read the character in from the BBB
        //if (Serial.available()) {
           desiredSpeed = Serial.parseInt();
        //}
        Serial.print(desiredSpeed);
        Serial.println();
        if (desiredSpeed == 0) {
          continueSpeed(lastSpeed);
        } else {
          //fadeFromSpeed(lastSpeed); 
          if (desiredSpeed > 0) {
              forw();
          } else {
              back();
          }   
           desiredSpeed = abs(desiredSpeed);
           desiredSpeed = min(desiredSpeed,255);
           fadeToSpeed(lastSpeed, desiredSpeed);
           lastSpeed = desiredSpeed;
           }
        } else {
           astop();
           bstop();
        } 
     }
}

/* Let's take a moment to talk about these functions. The forw and back functions are simply designating the direction the motors will turn once they are fed a PWM signal.
If you only call the forw, or back functions, you will not see the motors turn. On a similar note the fade in and out functions will only change PWM, so you need to consider
the direction you were last set to. In the code above, you might have noticed that I called forw and fade in the same grouping. You will want to call a new direction, and then
declare your pwm fade. There is also a stop function. 
*/

void forw() // no pwm defined
{ 
  digitalWrite(dir_a, HIGH);  //Reverse motor direction, 1 high, 2 low
  digitalWrite(dir_b, HIGH);  //Reverse motor direction, 3 low, 4 high  
}

void back() // no pwm defined
{
  digitalWrite(dir_a, LOW);  //Set motor direction, 1 low, 2 high
  digitalWrite(dir_b, LOW);  //Set motor direction, 3 high, 4 low
}

void forward() //full speed forward
{ 
  digitalWrite(dir_a, HIGH);  //Reverse motor direction, 1 high, 2 low
  digitalWrite(dir_b, HIGH);  //Reverse motor direction, 3 low, 4 high  
  analogWrite(pwm_a, 255);    //set both motors to run at (100/255 = 39)% duty cycle
  analogWrite(pwm_b, 255);
}

void backward() //full speed backward
{
  digitalWrite(dir_a, LOW);  //Set motor direction, 1 low, 2 high
  digitalWrite(dir_b, LOW);  //Set motor direction, 3 high, 4 low
  analogWrite(pwm_a, 255);   //set both motors to run at 100% duty cycle (fast)
  analogWrite(pwm_b, 255);
}

void stopped() //stop
{ 
  digitalWrite(dir_a, LOW); //Set motor direction, 1 low, 2 high
  digitalWrite(dir_b, LOW); //Set motor direction, 3 high, 4 low
  analogWrite(pwm_a, 0);    //set both motors to run at 100% duty cycle (fast)
  analogWrite(pwm_b, 0); 
}

void fadein()
{ 
  // fade in from min to max in increments of 5 points:
  for(int fadeValue = 0 ; fadeValue <= 255; fadeValue +=5) 
  { 
     // sets the value (range from 0 to 255):
    analogWrite(pwm_a, fadeValue);   
    analogWrite(pwm_b, fadeValue);    
    // wait for 30 milliseconds to see the dimming effect    
    delay(30);                            
  } 
}

void fadeToSpeed(int oldSpeed, int newSpeed)
{ 
  // fade in from min to max in increments of 5 points:
  if (newSpeed > oldSpeed){
    for(int fadeValue = oldSpeed ; fadeValue <= newSpeed; fadeValue +=5) 
    { 
       // sets the value (range from 0 to 255):
      analogWrite(pwm_a, fadeValue);   
      analogWrite(pwm_b, fadeValue);    
      // wait for 30 milliseconds to see the dimming effect    
      delay(30);                            
    } 
  } else {
    for(int fadeValue = oldSpeed ; fadeValue >= newSpeed; fadeValue -=5) 
    { 
       // sets the value (range from 0 to 255):
      analogWrite(pwm_a, fadeValue);   
      analogWrite(pwm_b, fadeValue);    
      // wait for 30 milliseconds to see the dimming effect    
      delay(30);                            
    }     
  }
}

void fadeout()
{ 
  // fade out from max to min in increments of 5 points:
  for(int fadeValue = 255 ; fadeValue >= 0; fadeValue -=5) 
  { 
    // sets the value (range from 0 to 255):
    analogWrite(pwm_a, fadeValue);
    analogWrite(pwm_b, fadeValue);
    // wait for 30 milliseconds to see the dimming effect    
    delay(30);  
}
}


void fadeFromSpeed(int maxSpeed)
{ 
  // fade out from max to min in increments of 5 points:
  for(int fadeValue = maxSpeed ; fadeValue >= 0; fadeValue -=5) 
  { 
    // sets the value (range from 0 to 255):
    analogWrite(pwm_a, fadeValue);
    analogWrite(pwm_b, fadeValue);
    // wait for 30 milliseconds to see the dimming effect    
    delay(30);  
  }
}

void continueSpeed(int spd) //full speed backward
{
  analogWrite(pwm_a, spd);   //set both motors to run at desiredSpeed
  analogWrite(pwm_b, spd);
}

void astop()                   //stop motor A
{
  analogWrite(pwm_a, 0);    //set both motors to run at 100% duty cycle (fast)
}

void bstop()                   //stop motor B
{ 
  analogWrite(pwm_b, 0);    //set both motors to run at 100% duty cycle (fast)
}
