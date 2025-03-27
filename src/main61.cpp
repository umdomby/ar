#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

const char* ssid = "Robolab124";
const char* password = "wifi123123123";
const char* websocket_server = "wss://ardu.site/ws";
const char* deviceId = "123";

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastHeartbeat2Time = 0;
bool wasConnected = false;
bool isIdentified = false;  // Добавляем флаг идентификации

// Motor pins
#define enA D6
#define in1 D2
#define in2 D7
#define enB D5
#define in3 D3
#define in4 D8

void sendLogMessage(const char* message) {
    if(client.available()) {
        StaticJsonDocument<128> doc;
        doc["type"] = "log";
        doc["message"] = message;
        doc["deviceId"] = deviceId;
        String output;
        serializeJson(doc, output);
        client.send(output);
    }
}

void sendCommandAck(const char* command) {
    if(client.available() && isIdentified) {  // Отправляем только после идентификации
        StaticJsonDocument<96> doc;
        doc["type"] = "command_ack";
        doc["command"] = command;
        doc["deviceId"] = deviceId;
        String output;
        serializeJson(doc, output);
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
    if(isIdentified) {  // Отправляем только если идентифицированы
        sendLogMessage("Motors stopped");
    }
}

void identifyDevice() {
    if(client.available()) {
        // 1. Отправляем тип клиента
        StaticJsonDocument<128> typeDoc;
        typeDoc["type"] = "client_type";
        typeDoc["clientType"] = "esp";
        String typeOutput;
        serializeJson(typeDoc, typeOutput);
        client.send(typeOutput);

        // 2. Отправляем идентификацию
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
    client.addHeader("Origin", "http://ardu.site");
    client.setInsecure(); // Для обхода проверки SSL
    
    if(client.connect(websocket_server)) {
        Serial.println("WebSocket connected!");
        wasConnected = true;
        isIdentified = false; // Сбрасываем флаг при новом подключении
        
        // Отправляем идентификацию
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
    if(error) return;

    // Проверяем сообщение об успешной идентификации
    if(doc["type"] == "system" && doc["status"] == "connected") {
        isIdentified = true;
        Serial.println("Successfully identified!");
        sendLogMessage("ESP connected and identified");
        return;
    }

    const char* command = doc["command"];
    if(!command) return;

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
    
    // Подключение к WiFi
    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    
    // Настройка WebSocket
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    
    // Первое подключение
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
    if(!client.available()) {
        if(millis() - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = millis();
            connectToServer();
        }
    } else {
        client.poll();

        // Heartbeat каждые 10 секунд (только после идентификации)
        if(isIdentified && millis() - lastHeartbeatTime > 10000) {
            lastHeartbeatTime = millis();
            sendLogMessage("Heartbeat - OK");
        }

        // Проверка Heartbeat2
        if(millis() - lastHeartbeat2Time > 2000) {
            stopMotors();
        }

        // Повторная идентификация, если еще не идентифицировались
        if(!isIdentified && millis() - lastReconnectAttempt > 2000) {
            identifyDevice();
        }
    }
}