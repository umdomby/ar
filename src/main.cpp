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
const char* deviceId = "444";

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastHeartbeat2Time = 0;
bool wasConnected = false;
bool isIdentified = false;

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

void sendLogMessage(const char* message) {
    if(client.available()) {
        StaticJsonDocument<256> doc;
        doc["type"] = "log";
        doc["message"] = message;
        doc["deviceId"] = deviceId;
        String output;
        serializeJson(doc, output);
        client.send(output);
    }
}

void sendCommandAck(const char* command) {
    if(client.available() && isIdentified) {
        StaticJsonDocument<256> doc;
        doc["type"] = "command_ack";
        doc["command"] = command;
        doc["deviceId"] = deviceId;
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
        typeDoc["type"] = "client_type";
        typeDoc["clientType"] = "esp";
        String typeOutput;
        serializeJson(typeDoc, typeOutput);
        client.send(typeOutput);

        StaticJsonDocument<128> doc;
        doc["type"] = "identify";
        doc["deviceId"] = deviceId;
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

    if(doc["type"] == "system" && doc["status"] == "connected") {
        isIdentified = true;
        Serial.println("Successfully identified!");
        sendLogMessage("ESP connected and identified");
        return;
    }

    const char* command = doc["command"];
    if(!command) return;

    else if(strcmp(command, "set_servo") == 0) {
        int angle = doc["params"]["angle"];
        // Жесткое ограничение угла
        if(angle < 0) {
            angle = 0;
            sendLogMessage("Warning: Servo angle clamped to 0 (was below minimum)");
        } else if(angle > 165) {
            angle = 165;
            sendLogMessage("Warning: Servo angle clamped to 180 (was above maximum)");
        }
        
        // Дополнительная проверка, чтобы не двигать серво, если угол не изменился
        if(angle != Servo1.read()) {
            startServoMove(angle, 500); // Плавное движение за 500 мс
            sendCommandAck("set_servo");
        }
    }
    if(strcmp(command, "motor_a_forward") == 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    }
    else if(strcmp(command, "motor_a_backward") == 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }
    else if(strcmp(command, "motor_b_forward") == 0) {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    }
    else if(strcmp(command, "motor_b_backward") == 0) {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }
    else if(strcmp(command, "set_speed") == 0) {
        const char* motor = doc["params"]["motor"];
        int speed = doc["params"]["speed"];
        if(strcmp(motor, "A") == 0) analogWrite(enA, speed);
        else if(strcmp(motor, "B") == 0) analogWrite(enB, speed);
    }
    else if(strcmp(command, "stop") == 0) {
        stopMotors();
    }
    else if(strcmp(command, "heartbeat2") == 0) {
        lastHeartbeat2Time = millis();
        return;
    }

    if(strcmp(command, "heartbeat2") != 0) {
        sendCommandAck(command);
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
    // Управление сервоприводом
    // if (!isServoMoving && millis() - lastServoMoveTime > 3000) {
    //     int newTarget = (servoTargetPosition == 180 || servoTargetPosition == 90) ? 0 : 180;
    //     startServoMove(newTarget, 1000);
    // }
    
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