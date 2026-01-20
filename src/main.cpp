#include <Arduino.h>
#include <ESP32Servo.h>

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

const int PIN_SERVO1 = 9;
const int PIN_SERVO2 = 10;
const int PIN_SERVO3 = 11;
const int PIN_SERVO4 = 12;

// ─── Функцию перемещаем наверх ────────────────────────────────
void moveServo(Servo &servo, const char* name)
{
  Serial.print(name);
  Serial.println(" → начинает движение");

  Serial.println("  90 → 0");
  for (int pos = 90; pos >= 0; pos -= 1) {
    servo.write(pos);
    delay(15);
  }
  delay(300);

  Serial.println("  0 → 180");
  for (int pos = 0; pos <= 180; pos += 1) {
    servo.write(pos);
    delay(15);
  }
  delay(400);

  Serial.println("  180 → 90");
  for (int pos = 180; pos >= 90; pos -= 1) {
    servo.write(pos);
    delay(15);
  }
  delay(500);

  Serial.println("  → вернулся в 90\n");
}

void setup() 
{
  Serial.begin(115200);
  delay(200);

  servo1.attach(PIN_SERVO1);
  servo2.attach(PIN_SERVO2);
  servo3.attach(PIN_SERVO3);
  servo4.attach(PIN_SERVO4);

  Serial.println("Старт последовательности 4 сервоприводов");

  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  servo4.write(90);
  delay(800);
}

void loop() 
{
  moveServo(servo1, "Servo 1");
  moveServo(servo2, "Servo 2");
  moveServo(servo3, "Servo 3");
  moveServo(servo4, "Servo 4");

  delay(600);
}