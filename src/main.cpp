#include <Arduino.h>
#include "ServoEasing.hpp"

ServoEasing myservo;  // Create servo object
int pos = 0;          // Variable to store the servo position
const int servoPin = D1;  // Servo connected to pin D1

void setup() {
  myservo.attach(servoPin);  // Attach servo to pin D1
  myservo.write(0);        // Initialize servo at 0 degrees
}

void loop() {
  // Slow rotation to 180 degrees
  for (pos = 0; pos <= 180; pos += 1) {
    myservo.write(pos);
    delay(15);  // Slow movement: 15ms delay per step
  }
  delay(2000);  // 2-second delay

  // Slow rotation back to 0 degrees
  for (pos = 180; pos >= 0; pos -= 1) {
    myservo.write(pos);
    delay(15);  // Slow movement: 15ms delay per step
  }
  delay(2000);  // 2-second delay

  // Fast rotation to +180 degrees
  myservo.write(180);
  delay(500);  // Fast movement: minimal delay
  delay(2000);  // 2-second delay

  // Fast rotation to -180 (0 degrees)
  myservo.write(0);
  delay(500);  // Fast movement: minimal delay
  delay(2000);  // 2-second delay
}