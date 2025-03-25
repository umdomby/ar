// #include <Arduino.h>
// #include <ESP8266WiFi.h>

// // // Двигатель A 
// // int enA = 9;
// // int in1 = 8;
// // int in2 = 7;
// // // Двигатель B
// // int enB = 3;
// // int in3 = 5;
// // int in4 = 4;

// #define enA D6
// #define in1 D2
// #define in2 D7

// #define enB D5
// #define in3 D3
// #define in4 D8


 
// void setup()
// {
//   pinMode(enA, OUTPUT);
//   pinMode(enB, OUTPUT);
//   pinMode(in1, OUTPUT);
//   pinMode(in2, OUTPUT);
//   pinMode(in3, OUTPUT);
//   pinMode(in4, OUTPUT);
//   digitalWrite(in1, LOW);
//   digitalWrite(in2, LOW);
//   digitalWrite(in3, LOW);
//   digitalWrite(in4, LOW);
// }
 
// void loop()
// {
// // Установка двигателя A и B на максимальную скорость (0 ... 255)
//   analogWrite(enA, 255);   
//   analogWrite(enB, 255);
// // Вращение двигателем A и B вперед
//   digitalWrite(in1, HIGH);
//   digitalWrite(in2, LOW);
//   digitalWrite(in3, HIGH);
//   digitalWrite(in4, LOW);
//   delay(5000);
// // Вращение двигателем A и B назад
//  digitalWrite(in1, LOW);
//  digitalWrite(in2, HIGH);
//  digitalWrite(in3, LOW);
//  digitalWrite(in4, HIGH);
//  delay(5000);

// // Вращение двигателем A назад B вперед
//  digitalWrite(in1, LOW);
//  digitalWrite(in2, HIGH);
//  digitalWrite(in3, HIGH);
//  digitalWrite(in4, LOW);
//  delay(5000);

//  // Вращение двигателем A вперёд B назад
//  digitalWrite(in1, HIGH);
//  digitalWrite(in2, LOW);
//  digitalWrite(in3, LOW);
//  digitalWrite(in4, HIGH);
//  delay(5000);


// //Отключение мотора A и B
//  digitalWrite(in1, LOW);
//  digitalWrite(in2, LOW);
//  digitalWrite(in3, LOW);
//  digitalWrite(in4, LOW);
// }