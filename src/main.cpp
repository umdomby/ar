#include <Arduino.h>

// Пины для управления одним мотором (BTS7960)
#define PIN_EN   D1   // PWM (скорость)
#define PIN_IN1  D2   // Направление 1
#define PIN_IN2  D3   // Направление 2

const int MOTOR_SPEED = 50;  // 0..255, примерно 60% мощности

void motorForward(int speed);
void motorBackward(int speed);
void motorStop();

void setup() 
{
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);

  motorStop();           // на всякий случай выключаем сразу

  Serial.begin(115200);
  delay(300);
  Serial.println("\nТест одного мотора BTS7960 начат\n");
  
  Serial.println("Вперёд 3 секунды...");
  motorForward(MOTOR_SPEED);
  delay(3000);

  Serial.println("Стоп 2 секунды");
  motorStop();
  delay(2000);

  Serial.println("Назад 3 секунды...");
  motorBackward(MOTOR_SPEED);
  delay(3000);

  Serial.println("Стоп");
  motorStop();
}

void loop() 
{
  // Для постоянного цикла раскомментируйте нужное:

  Serial.println("Вперёд → стоп → назад → стоп");
  motorForward(MOTOR_SPEED);
  delay(4000);
  
  motorStop();
  delay(1500);
  
  motorBackward(MOTOR_SPEED);
  delay(4000);
  
  motorStop();
  delay(1500);

}

// ------------------ Функции управления мотором ------------------

void motorForward(int speed)
{
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_EN, speed);
}

void motorBackward(int speed)
{
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  analogWrite(PIN_EN, speed);
}

void motorStop()
{
  analogWrite(PIN_EN, 0);
  // Можно и так (более "жёсткий" стоп):
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
}