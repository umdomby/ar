#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "ServoEasing.hpp"

unsigned long lastWiFiCheck = 0;
unsigned long disconnectStartTime = 0;
const unsigned long MAX_DISCONNECT_TIME = 20UL * 60UL * 60UL * 1000UL; // 10 часов в миллисекундах

const int analogPin = A0;

// Motor pins
#define enA D1
#define in1 D2
#define in2 D3
#define in3 D4
#define in4 D5
#define enB D6

// relay pins
#define button1 D0
#define button2 3  // Реле 2 на пине RX (GPIO3)

// servo pins
#define SERVO1_PIN D7 // ось Y rightStick
#define SERVO2_PIN D8 // ось X leftStick
ServoEasing Servo1;
ServoEasing Servo2;

using namespace websockets;

const char *ssid = "Robolab124";
const char *password = "wifi123123123";
const char *websocket_server = "wss://ardua.site:444/wsar";
const char *de = "4444444444444444"; // deviceId → de

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastHeartbeat2Time = 0;
bool wasConnected = false;
bool isIdentified = false;

void sendCommandAck(const char *co, int sp = -1); // command → co, speed → sp
void onMessageCallback(WebsocketsMessage message);
void onEventsCallback(WebsocketsEvent event, String data);
// Отправка логов
void sendLogMessage(const char *me)
{
    if (client.available())
    {
        StaticJsonDocument<256> doc;
        doc["ty"] = "log";
        doc["me"] = me;
        doc["de"] = de;
        doc["b1"] = digitalRead(button1) == LOW ? "on" : "off"; // Состояние реле 1
        doc["b2"] = digitalRead(button2) == LOW ? "on" : "off"; // Состояние реле 2
        doc["sp1"] = Servo1.read(); // Угол первого сервопривода
        doc["sp2"] = Servo2.read(); // Угол второго сервопривода
        int raw = analogRead(analogPin); // Чтение с A0 (0–1023)
        float inputVoltage = raw * 0.021888; // Преобразование в напряжение
        char voltageStr[8];
        dtostrf(inputVoltage, 5, 2, voltageStr); // Форматируем в строку с 2 знаками после запятой
        doc["z"] = voltageStr; // Добавляем отформатированное значение как z
        String output;
        serializeJson(doc, output);
        client.send(output);
    }
}

// Подтверждение команды
void sendCommandAck(const char *co, int sp)
{
    if (client.available() && isIdentified)
    {
        StaticJsonDocument<256> doc;
        doc["ty"] = "ack";
        doc["co"] = co;
        doc["de"] = de;
        if (strcmp(co, "SPD") == 0 && sp != -1)
        {
            doc["sp"] = sp;
        }
        String output;
        serializeJson(doc, output);
        Serial.println("Sending ack: " + output); // Отладка
        client.send(output);
    }
}

void stopMotors()
{
    analogWrite(enA, 0);
    analogWrite(enB, 0);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    // if (isIdentified)
    // {
    //     sendLogMessage("Motors stopped");
    // }
}

void identifyDevice()
{
    if (client.available())
    {
        StaticJsonDocument<128> typeDoc;
        typeDoc["ty"] = "clt"; // type → ty, client_type → clt
        typeDoc["ct"] = "esp"; // clientType → ct
        String typeOutput;
        serializeJson(typeDoc, typeOutput);
        client.send(typeOutput);

        StaticJsonDocument<128> doc;
        doc["ty"] = "idn"; // type → ty, identify → idn
        doc["de"] = de;    // deviceId → de
        String output;
        serializeJson(doc, output);
        client.send(output);

        Serial.println("Identification sent");
    }
}

void ensureWiFiConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi reconnected");
        } else {
            Serial.println("\nWiFi reconnection failed");
        }
    }
}

void connectToServer() {
    Serial.println("Connecting to server...");
    client.close(); // Закрываем старое соединение
    client = WebsocketsClient(); // Создаем новый экземпляр клиента
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    client.addHeader("Origin", "http://ardua.site");
    client.setInsecure();

    if (client.connect(websocket_server)) {
        Serial.println("WebSocket connected!");
        wasConnected = true;
        isIdentified = false;
        disconnectStartTime = 0; // Сбрасываем время отключения
        identifyDevice();
    } else {
        Serial.println("WebSocket connection failed!");
        wasConnected = false;
        isIdentified = false;
        if (disconnectStartTime == 0) {
            disconnectStartTime = millis(); // Запоминаем время начала отключения
        }
    }
}

// Обработка входящих сообщений
void onMessageCallback(WebsocketsMessage message)
{
    StaticJsonDocument<192> doc;
    DeserializationError error = deserializeJson(doc, message.data());

    if (error)
    {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    lastHeartbeat2Time = millis();
    
    Serial.println("Received message: " + message.data()); // Отладка

    if (doc["ty"] == "sys" && doc["st"] == "con")
    {
        isIdentified = true;
        Serial.println("Successfully identified!");
        sendLogMessage("ESP connected and identified");

        // Отправка состояния реле после идентификации
        char relayStatus[64];
        snprintf(relayStatus, sizeof(relayStatus), "Relay states: D0=%s, 3=%s",
                 digitalRead(button1) == LOW ? "on" : "off",
                 digitalRead(button2) == LOW ? "on" : "off");
        sendLogMessage(relayStatus);

        // Установить начальные углы сервоприводов
        Servo1.write(90);
        Servo2.write(90);
        //sendLogMessage("Servos initialized to 90 degrees");
        return;
    }

    const char *co = doc["co"];
    if (!co)
        return;

    if (strcmp(co, "GET_RELAYS") == 0)
    {
        char relayStatus[64];
        snprintf(relayStatus, sizeof(relayStatus), "Relay states: D0=%s, 3=%s",
                 digitalRead(button1) == LOW ? "on" : "off",
                 digitalRead(button2) == LOW ? "on" : "off");
        sendLogMessage(relayStatus);
        return;
    }
    
    //control axis
    if (strcmp(co, "SSR") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180); // Ограничение угла 0–180
        if (an != Servo1.read())
        {
            Servo1.write(an);
            sendCommandAck("SSR");
            char logMsg[32];
            snprintf(logMsg, sizeof(logMsg), "Servo1 set to %d degrees", an);
            //sendLogMessage(logMsg);
        }
    }
    else if (strcmp(co, "SSR2") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180); // Ограничение угла 0–180
        if (an != Servo2.read())
        {
            Servo2.write(an);
            sendCommandAck("SSR2");
            char logMsg[32];
            snprintf(logMsg, sizeof(logMsg), "Servo2 set to %d degrees", an);
            //sendLogMessage(logMsg);
        }
    }
    else if (strcmp(co, "MFA") == 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        sendCommandAck("MFA");
    }
    else if (strcmp(co, "MRA") == 0)
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        sendCommandAck("MRA");
    }
    else if (strcmp(co, "MFB") == 0)
    {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
        sendCommandAck("MFB");
    }
    else if (strcmp(co, "MRB") == 0)
    {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
        sendCommandAck("MRB");
    }
    else if (strcmp(co, "SPD") == 0)
    {
        const char *mo = doc["pa"]["mo"];
        int speed = doc["pa"]["sp"];
        if (strcmp(mo, "A") == 0)
        {
            analogWrite(enA, speed);
            sendCommandAck("SPD", speed);
        }
        else if (strcmp(mo, "B") == 0)
        {
            analogWrite(enB, speed);
            sendCommandAck("SPD", speed);
        }
    }
    else if (strcmp(co, "STP") == 0)
    {
        stopMotors();
        sendCommandAck("STP");
    }
    else if (strcmp(co, "HBT") == 0)
    {
        lastHeartbeat2Time = millis();
        //sendLogMessage("Heartbeat - OK");
        //return;
    }
    else if (strcmp(co, "RLY") == 0)
    {
        const char *pin = doc["pa"]["pin"];
        const char *state = doc["pa"]["state"];

        if (strcmp(pin, "D0") == 0)
        {
            digitalWrite(button1, strcmp(state, "on") == 0 ? LOW : HIGH);
            Serial.println("Relay 1 (D0) set to: " + String(digitalRead(button1)));
            sendLogMessage(strcmp(state, "on") == 0 ? "Реле 1 (D0) включено" : "Реле 1 (D0) выключено");
        }
        else if (strcmp(pin, "3") == 0)
        {
            digitalWrite(button2, strcmp(state, "on") == 0 ? LOW : HIGH);
            Serial.println("Relay 2 (3) set to: " + String(digitalRead(button2)));
            sendLogMessage(strcmp(state, "on") == 0 ? "Реле 2 (3) включено" : "Реле 2 (3) выключено");
        }

        StaticJsonDocument<256> ackDoc;
        ackDoc["ty"] = "ack";
        ackDoc["co"] = "RLY";
        ackDoc["de"] = de;
        JsonObject pa = ackDoc.createNestedObject("pa");
        pa["pin"] = pin;
        pa["state"] = digitalRead(strcmp(pin, "D0") == 0 ? button1 : button2) ? "on" : "off";

        String output;
        serializeJson(ackDoc, output);
        Serial.println("Sending ack: " + output);
        client.send(output);
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connection opened");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connection closed");
        if (wasConnected) {
            wasConnected = false;
            isIdentified = false;
            stopMotors();
        }
        if (disconnectStartTime == 0) {
            disconnectStartTime = millis(); // Запоминаем время начала отключения
        }
    } else if (event == WebsocketsEvent::GotPing) {
        client.pong();
    }
}

// Инициализация
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP8266...");

    // Инициализация первого сервопривода
    if (Servo1.attach(SERVO1_PIN, 90) == INVALID_SERVO)
    {
        Serial.println("Error attaching servo");
        while (1)
            delay(100);
    }
    Servo1.write(90);

    // Инициализация второго сервопривода
    if (Servo2.attach(SERVO2_PIN, 90) == INVALID_SERVO)
    {
        Serial.println("Error attaching servo2");
        while (1)
            delay(100);
    }
    Servo2.write(90);

    // Подключение к WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Настройка WebSocket
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    connectToServer();

    // Инициализация моторов и реле
    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);
    pinMode(button1, OUTPUT);
    pinMode(button2, OUTPUT);
    digitalWrite(button1, LOW);
    digitalWrite(button2, LOW);
    stopMotors();
    Serial.println("Motors and relays initialized");
}

void loop() {
    // Проверка WiFi каждые 30 секунд
    if (millis() - lastWiFiCheck > 30000) {
        lastWiFiCheck = millis();
        ensureWiFiConnected();
    }

    // Работа с WebSocket
    if (!client.available()) {
        if (millis() - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = millis();
            connectToServer();
        }

        // Проверка длительного отключения (20 часов)
        if (disconnectStartTime > 0 && (millis() - disconnectStartTime > MAX_DISCONNECT_TIME)) {
            Serial.println("No connection for 20 hours, restarting...");
            ESP.restart(); // Программный перезапуск
        }
    } else {
        client.poll();

        if (isIdentified) {
            if (millis() - lastHeartbeatTime > 5000) {
                lastHeartbeatTime = millis();
                sendLogMessage("Heartbeat - OK");
                char relayStatus[64];
                snprintf(relayStatus, sizeof(relayStatus), "Relay states: D0=%s, 3=%s",
                         digitalRead(button1) == LOW ? "on" : "off",
                         digitalRead(button2) == LOW ? "on" : "off");
                //sendLogMessage(relayStatus);
            }

            if (millis() - lastHeartbeat2Time > 700) {
                stopMotors();
            }
        } else if (millis() - lastReconnectAttempt > 3000) {
            lastReconnectAttempt = millis();
            identifyDevice();
        }
    }
}