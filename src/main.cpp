// Полное → Сокращенное
// type → ty
// status → st
// command → co
// angle → an
// speed → sp
// motor → mo
// message → me
// deviceId → de
// params → pa
// timestamp → ts
// origin → or
// reason → re
// clientType → ct

// Команды:
// motor_a_forward → MFA
// motor_a_backward → MRA
// motor_b_forward → MFB
// motor_b_backward → MRB
// set_speed → SPD
// set_servo → SSR
// stop → STP
// heartbeat2 → HBT

// Типы сообщений:
// system → sys
// error → err
// log → log
// acknowledge → ack
// client_type → clt
// identify → idn
// esp_status → est
// command_status → cst

// Статусы:
// connected → con
// disconnected → dis
// awaiting_identification → awi
// rejected → rej
// delivered → dvd
// esp_not_found → enf

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "ServoEasing.hpp"

#define SERVO1_PIN D1
ServoEasing Servo1;

using namespace websockets;

const char* ssid = "Robolab124";
const char* password = "wifi123123123";
const char* websocket_server = "wss://ardua.site/wsar";
const char* de = "YNNGUT123PP5KMNB"; // deviceId → de

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastHeartbeat2Time = 0;
bool wasConnected = false;
bool isIdentified = false;
void sendCommandAck(const char* co, int sp = -1); // command → co, speed → sp
// Motor pins
#define enA D6
#define in1 D2
#define in2 D7
#define enB D5
#define in3 D3
#define in4 D8

// Servo control
unsigned long lastServoMoveTime = 0;
int servoTargetPosition = 90; // Начальная позиция
bool isServoMoving = false;
unsigned long servoMoveStartTime = 0;
int servoStartPosition = 90;
unsigned long servoMoveDuration = 1000; // Длительность движения по умолчанию

void startServoMove(int targetPos, unsigned long duration) {
if (isServoMoving) return;

    // Дополнительная проверка на допустимый диапазон (на всякий случай)
    targetPos = constrain(targetPos, 0, 165);
    
    servoStartPosition = Servo1.read();
    servoTargetPosition = targetPos;
    servoMoveDuration = duration;
    servoMoveStartTime = millis();
    isServoMoving = true;
    
    Servo1.setSpeed(60); // Убедитесь, что скорость адекватная
    Servo1.easeTo(servoTargetPosition, servoMoveDuration);
}

void updateServoPosition() {
if (isServoMoving && !Servo1.isMoving()) {
isServoMoving = false;
lastServoMoveTime = millis();
}
}

void sendLogMessage(const char* me) { // message → me
if(client.available()) {
StaticJsonDocument<256> doc;
doc["ty"] = "log"; // type → ty
doc["me"] = me; // message → me
doc["de"] = de; // deviceId → de
String output;
serializeJson(doc, output);
client.send(output);
}
}

void sendCommandAck(const char* co, int sp) { // command → co
if(client.available() && isIdentified) {
StaticJsonDocument<256> doc;
doc["ty"] = "ack"; // type → ty, acknowledge → ack
doc["co"] = co; // command → co
doc["de"] = de; // deviceId → de

        // Добавляем скорость в лог только для команд SPD и если скорость указана
        if (strcmp(co, "SPD") == 0 && sp != -1) { // set_speed → SPD
            doc["sp"] = sp; // speed → sp
        }
String output;
serializeJson(doc, output);
Serial.println("Sending device ID: " + output);  // Логируем отправляемый ID
client.send(output);
}
}

void stopMotors() {
analogWrite(enA, 0);
analogWrite(enB, 0);
digitalWrite(in1, LOW);
digitalWrite(in2, LOW);
digitalWrite(in3, LOW);
digitalWrite(in4, LOW);
if(isIdentified) {
sendLogMessage("Motors stopped");
}
}

void identifyDevice() {
if(client.available()) {
StaticJsonDocument<128> typeDoc;
typeDoc["ty"] = "clt"; // type → ty, client_type → clt
typeDoc["ct"] = "esp"; // clientType → ct
String typeOutput;
serializeJson(typeDoc, typeOutput);
client.send(typeOutput);

        StaticJsonDocument<128> doc;
        doc["ty"] = "idn"; // type → ty, identify → idn
        doc["de"] = de; // deviceId → de
        String output;
        serializeJson(doc, output);
        client.send(output);

        Serial.println("Identification sent");
    }
}

void connectToServer() {
Serial.println("Connecting to server...");
client.addHeader("Origin", "http://ardua.site");
client.setInsecure();

    if(client.connect(websocket_server)) {
        Serial.println("WebSocket connected!");
        wasConnected = true;
        isIdentified = false;
        identifyDevice();
    } else {
        Serial.println("WebSocket connection failed!");
        wasConnected = false;
        isIdentified = false;
    }
}

void onMessageCallback(WebsocketsMessage message) {
StaticJsonDocument<192> doc;
DeserializationError error = deserializeJson(doc, message.data());
if(error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
}

    if(doc["ty"] == "sys" && doc["st"] == "con") { // type → ty, system → sys, status → st, connected → con
        isIdentified = true;
        Serial.println("Successfully identified!");
        sendLogMessage("ESP connected and identified");
        return;
    }

    const char* co = doc["co"]; // command → co
    if(!co) return;

    else if(strcmp(co, "SSR") == 0) { // set_servo → SSR
        int an = doc["pa"]["an"]; // params → pa, angle → an
        // Жесткое ограничение угла
        if(an < 0) {
            an = 0;
            sendLogMessage("Warning: Servo angle clamped to 0 (was below minimum)");
        } else if(an > 165) {
            an = 165;
            sendLogMessage("Warning: Servo angle clamped to 180 (was above maximum)");
        }
        
        // Дополнительная проверка, чтобы не двигать серво, если угол не изменился
        if(an != Servo1.read()) {
            startServoMove(an, 500); // Плавное движение за 500 мс
            sendCommandAck("SSR"); // set_servo → SSR
        }
    }
    if(strcmp(co, "MFA") == 0) { // motor_a_forward → MFA
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    }
    else if(strcmp(co, "MRA") == 0) { // motor_a_backward → MRA
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }
    else if(strcmp(co, "MFB") == 0) { // motor_b_forward → MFB
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    }
    else if(strcmp(co, "MRB") == 0) { // motor_b_backward → MRB
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }
    else if(strcmp(co, "SPD") == 0) { // set_speed
        const char* mo = doc["pa"]["mo"]; // motor
        int speed = doc["pa"]["sp"];     // speed
        
        if(strcmp(mo, "A") == 0) {
            analogWrite(enA, speed);
            sendCommandAck("SPD", speed); // Отправляем с текущей скоростью
        }
        else if(strcmp(mo, "B") == 0) {
            analogWrite(enB, speed);
            sendCommandAck("SPD", speed); // Отправляем с текущей скоростью
        }
    }
    else if(strcmp(co, "STP") == 0) { // stop → STP
        stopMotors();
    }
    else if(strcmp(co, "HBT") == 0) { // heartbeat2 → HBT
        lastHeartbeat2Time = millis();
        return;
    }

    if(strcmp(co, "HBT") != 0) { // heartbeat2 → HBT
        sendCommandAck(co); // command → co
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
if(event == WebsocketsEvent::ConnectionOpened) {
Serial.println("Connection opened");
}
else if(event == WebsocketsEvent::ConnectionClosed) {
Serial.println("Connection closed");
if(wasConnected) {
wasConnected = false;
isIdentified = false;
stopMotors();
}
}
else if(event == WebsocketsEvent::GotPing) {
client.pong();
}
}

void setup() {
Serial.begin(115200);
delay(1000); // Даем время для инициализации Serial

    // Инициализация сервопривода
    if (Servo1.attach(SERVO1_PIN, 90) == INVALID_SERVO) {
        Serial.println("Error attaching servo");
        while(1) delay(100);
    }
    Servo1.setSpeed(60);
    Servo1.write(90); // Устанавливаем начальное положение

    // Подключение к WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    
    // Настройка WebSocket
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    connectToServer();

    // Инициализация моторов
    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);
    stopMotors();
}

void loop() {
    updateServoPosition();

    // Работа с WebSocket
    if(!client.available()) {
        if(millis() - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = millis();
            connectToServer();
        }
    } else {
        client.poll();

        if(isIdentified) {
            if(millis() - lastHeartbeatTime > 10000) {
                lastHeartbeatTime = millis();
                sendLogMessage("Heartbeat - OK");
            }
            
            if(millis() - lastHeartbeat2Time > 2000) {
                stopMotors();
            }
        } else if(millis() - lastReconnectAttempt > 3000) {
            lastReconnectAttempt = millis();
            identifyDevice();
        }
    }
}