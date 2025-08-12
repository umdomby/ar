#include <Arduino.h>
#include "ServoEasing.hpp"

ServoEasing myservo;  // Создаём объект сервопривода
const int servoPin = D1;  // Сервопривод подключён к пину D1

void setup() {
  myservo.attach(servoPin);  // Подключаем сервопривод к пину D1
  myservo.write(90);        // Устанавливаем сервопривод в 90 градусов
  delay(1000);             // Задержка 1 секунда для стабилизации
}

void loop() {
  // Пустой цикл, сервопривод остаётся в 90 градусах
}