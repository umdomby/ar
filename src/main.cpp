#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "ServoEasing.hpp"

unsigned long lastWiFiCheck = 0;
unsigned long disconnectStartTime = 0;
const unsigned long MAX_DISCONNECT_TIME = 1UL * 60UL * 60UL * 1000UL; // 1 час

const int analogPin = A0;

// Motor pins driver BTS7960
#define enA D1
#define in1 D2
#define in2 D3
#define in3 D4
#define in4 D5
#define enB D6

// Только одно реле — на D0 (GPIO16)
#define button2 D0  // GPIO16 - простое реле

// Servo pins
#define SERVO1_PIN D7
#define SERVO2_PIN D8

ServoEasing Servo1;
ServoEasing Servo2;

using namespace websockets;

const char *ssid = "Robolab124";
const char *password = "wifi123123123";
const char *websocket_server = "wss://ardua.site:444/wsar";

String alarm = "off";
boolean alarmMotion = false;

const char *de = "9999999999999999"; // deviceId

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastAnalogReadTime = 0;
unsigned long lastHeartbeat2Time = 0;
bool wasConnected = false;
bool isIdentified = false;

// Объявления функций
void sendLogMessage(const char *me);
void sendCommandAck(const char *co, int sp = -1);
void stopMotors();
void identifyDevice();
void ensureWiFiConnected();
void connectToServer();
void onMessageCallback(WebsocketsMessage message);
void onEventsCallback(WebsocketsEvent event, String data);

void sendLogMessage(const char *me)
{
    if (client.available())
    {
        StaticJsonDocument<256> doc;
        doc["ty"] = "log";
        doc["me"] = me;
        doc["de"] = de;
        doc["b2"] = digitalRead(button2) == LOW ? "off" : "on";  // Только реле на D0
        doc["sp1"] = Servo1.read();
        doc["sp2"] = Servo2.read();
        int raw = analogRead(analogPin);
        float inputVoltage = raw * 0.021888;
        char voltageStr[8];
        dtostrf(inputVoltage, 5, 2, voltageStr);
        doc["z"] = voltageStr;
        doc["r"] = "Dionis-Moto";
        doc["a"] = alarm;
        doc["m"] = alarmMotion;
        String output;
        serializeJson(doc, output);
        Serial.println("sendLogMessage: " + output);
        client.send(output);
        alarmMotion = false;
    }
}

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
        Serial.println("Sending ack: " + output);
        client.send(output);
    }
}

void stopMotors()
{
    analogWrite(enA, 0);
    analogWrite(enB, 0);
}

void identifyDevice()
{
    if (client.available())
    {
        StaticJsonDocument<128> typeDoc;
        typeDoc["ty"] = "clt";
        typeDoc["ct"] = "esp";
        String typeOutput;
        serializeJson(typeDoc, typeOutput);
        client.send(typeOutput);

        StaticJsonDocument<128> doc;
        doc["ty"] = "idn";
        doc["de"] = de;
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
    client.close();
    client = WebsocketsClient();
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    client.addHeader("Origin", "http://ardua.site");
    client.setInsecure();

    if (client.connect(websocket_server)) {
        Serial.println("WebSocket connected!");
        wasConnected = true;
        isIdentified = false;
        disconnectStartTime = 0;
        identifyDevice();
    } else {
        Serial.println("WebSocket connection failed!");
        wasConnected = false;
        isIdentified = false;
        if (disconnectStartTime == 0) {
            disconnectStartTime = millis();
        }
    }
}

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

    Serial.println("Received message: " + message.data());

    if (doc["ty"] == "sys" && doc["st"] == "con")
    {
        isIdentified = true;
        Serial.println("Successfully identified!");
        sendLogMessage("ESP connected and identified");

        char relayStatus[64];
        snprintf(relayStatus, sizeof(relayStatus), "Relay D0=%s",
                 digitalRead(button2) == LOW ? "off" : "on");
        sendLogMessage(relayStatus);

        Servo1.write(90);
        Servo2.write(90);
        sendLogMessage("Servos initialized to 90 degrees");
        return;
    }

    const char *co = doc["co"];
    if (!co) return;

    if (strcmp(co, "STP") == 0){
        stopMotors();
    }
    else if (strcmp(co, "SPD") == 0) {
        const char *mo = doc["pa"]["mo"];
        int speed = doc["pa"]["sp"];
        Serial.printf("SPD command received: motor=%s, speed=%d\n", mo, speed);
        if (strcmp(mo, "A") == 0) {
            analogWrite(enA, speed);
        } else if(strcmp(mo, "B") == 0) {
            analogWrite(enB, speed);
        }
        sendLogMessage("SPD");
    }
    else if (strcmp(co, "MFA") == 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    }
    else if (strcmp(co, "MRA") == 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    }
    else if (strcmp(co, "MFB") == 0) {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
    }
    else if (strcmp(co, "MRB") == 0) {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
    }
    else if (strcmp(co, "SAR") == 0)
    {
        int an = doc["pa"]["an"];
        int ak = doc["pa"]["ak"];
        an = constrain(an, 0, 180);
        ak = constrain(ak, 0, 180);
        Servo1.write(an);
        Servo2.write(ak);
    }
    else if (strcmp(co, "SSY") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180);
        Servo1.write(an);
    }
    else if (strcmp(co, "SSX") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180);
        Servo2.write(an);
    }
    else if (strcmp(co, "SSA") == 0)
    {
        int an = doc["pa"]["an"];
        int SSA = Servo1.read();
        if(an > 0){
            if(SSA + an < 180) Servo1.write(SSA + an);
        }else{
            if(SSA + an > 0) Servo1.write(SSA + an);
        }
    }
    else if (strcmp(co, "SSB") == 0)
    {
        int an = doc["pa"]["an"];
        int SSB = Servo2.read();
        if(an > 0){
            if(SSB + an < 180) Servo2.write(SSB + an);
        }else{
            if(SSB + an > 0) Servo2.write(SSB + an);
        }
    }
    else if (strcmp(co, "GET_RELAYS") == 0)
    {
        char relayStatus[64];
        snprintf(relayStatus, sizeof(relayStatus), "Relay D0=%s",
                 digitalRead(button2) == LOW ? "off" : "on");
        sendLogMessage(relayStatus);
        return;
    }
    else if (strcmp(co, "HBT") == 0)
    {
        lastHeartbeat2Time = millis();
    }
    else if (strcmp(co, "RLY") == 0)
    {
        const char *pin = doc["pa"]["pin"];
        const char *state = doc["pa"]["state"];

        if (!pin || !state) {
            Serial.println("Ошибка: pin или state отсутствуют в JSON!");
            return;
        }

        // Теперь только D0
        if (strcmp(pin, "D0") == 0)
        {
            digitalWrite(button2, strcmp(state, "on") == 0 ? LOW : HIGH);
            Serial.println("Relay D0 set to: " + String(state));
        }

        StaticJsonDocument<256> ackDoc;
        ackDoc["ty"] = "ack";
        ackDoc["co"] = "RLY";
        ackDoc["de"] = de;
        JsonObject pa = ackDoc.createNestedObject("pa");
        pa["pin"] = pin;
        pa["state"] = state;
        String output;
        serializeJson(ackDoc, output);
        Serial.println("Отправка подтверждения RLY: " + output);
        client.send(output);
    }
    else if (strcmp(co, "ALARM") == 0)
    {
        const char *state = doc["pa"]["state"];
        if (!state) return;

        alarm = state;

        StaticJsonDocument<256> ackDoc;
        ackDoc["ty"] = "ack";
        ackDoc["co"] = "ALARM";
        ackDoc["de"] = de;
        JsonObject pa = ackDoc.createNestedObject("pa");
        pa["state"] = alarm;
        String output;
        serializeJson(ackDoc, output);
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
            disconnectStartTime = millis();
        }
    } else if (event == WebsocketsEvent::GotPing) {
        client.pong();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP8266...");

    if (Servo1.attach(SERVO1_PIN, 90) == INVALID_SERVO)
    {
        Serial.println("Error attaching servo1");
        while (1) delay(100);
    }
    Servo1.write(90);

    if (Servo2.attach(SERVO2_PIN, 90) == INVALID_SERVO)
    {
        Serial.println("Error attaching servo2");
        while (1) delay(100);
    }
    Servo2.write(90);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    connectToServer();

    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(in3, OUTPUT);
    pinMode(in4, OUTPUT);
    pinMode(button2, OUTPUT);  // Только D0

    digitalWrite(button2, HIGH);
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    stopMotors();
    Serial.println("Motors and relay initialized");
}

void loop() {
    if (millis() - lastWiFiCheck > 30000) {
        lastWiFiCheck = millis();
        ensureWiFiConnected();
    }

    if (!client.available()) {
        if (millis() - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = millis();
            connectToServer();
        }

        if (disconnectStartTime > 0 && (millis() - disconnectStartTime > MAX_DISCONNECT_TIME)) {
            Serial.println("No connection for 1 hour, restarting...");
            ESP.restart();
        }
    } else {
        client.poll();

        if (isIdentified) {
            if (millis() - lastAnalogReadTime > 100) {
                lastAnalogReadTime = millis();
            }

            if (millis() - lastHeartbeatTime > 5000) {
                lastHeartbeatTime = millis();
                sendLogMessage("HBT");
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