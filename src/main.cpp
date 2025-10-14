#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "ServoEasing.hpp"

unsigned long lastWiFiCheck = 0;
unsigned long disconnectStartTime = 0;
const unsigned long MAX_DISCONNECT_TIME = 20UL * 60UL * 60UL * 1000UL; // 10 часов в миллисекундах

const int analogPin = A0;

// Motor pins driver BTS7960
#define enA D1
#define in1 D2
#define in2 D3
#define in3 D4
#define in4 D5
#define enB D6

// relay pins
#define button1 3   // lightе RX GPIO3)
#define button2 D0  // alarm + charger
//#define button3 1   // lightе TX GPIO1)

// servo pins
#define SERVO1_PIN D7 // ось Y rightStick
#define SERVO2_PIN D8 // ось X leftStick
ServoEasing Servo1;
ServoEasing Servo2;

using namespace websockets;

const char *ssid = "Robolab124";
const char *password = "wifi123123123";
const char *websocket_server = "wss://ardua.site:444/wsar";

const char *de = "9999999999999999"; // deviceId → de
//const char *de = "4444444444444444"; // deviceId → de

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastMillisAlarm = 0;
unsigned long lastMillisAxisJoyX = 0;
unsigned long lastMillisAxisJoyY = 0;
unsigned long lastAnalogReadTime = 0;
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
        Serial.println("sendLogMessage: " + output); // Отладка
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
    // digitalWrite(in1, LOW);
    // digitalWrite(in2, LOW);
    // digitalWrite(in3, LOW);
    // digitalWrite(in4, LOW);
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
        sendLogMessage("Servos initialized to 90 degrees");
        return;
    }

    const char *co = doc["co"];
    if (!co)
        return;

    if (strcmp(co, "STP") == 0){
        stopMotors();
        //sendCommandAck("STP");
    }

    else if (strcmp(co, "SPD") == 0) {
        const char *mo = doc["pa"]["mo"];
        int speed = doc["pa"]["sp"];
        Serial.printf("SPD command received: motor=%s, speed=%d\n", mo, speed);
        if (strcmp(mo, "A") == 0) {
            analogWrite(enA, speed);
            Serial.printf("Setting enA to %d\n", speed);
        } else if (strcmp(mo, "B") == 0) {
            analogWrite(enB, speed);
            Serial.printf("Setting enB to %d\n", speed);
        }
        sendLogMessage("SPD");
    }
    else if (strcmp(co, "MFA") == 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        Serial.println("MFA: in1=HIGH, in2=LOW");
    }
    else if (strcmp(co, "MRA") == 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        Serial.println("MRA: in1=LOW, in2=HIGH");
    }
    else if (strcmp(co, "MFB") == 0) {
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
        Serial.println("MFB: in3=HIGH, in4=LOW");
    }
    else if (strcmp(co, "MRB") == 0) {
        digitalWrite(in3, LOW);
        digitalWrite(in4, HIGH);
        Serial.println("MRB: in3=LOW, in4=HIGH");
    }

    //control axisVB
    else if (strcmp(co, "SAR") == 0)
    {
        int an = doc["pa"]["an"];
        int ak = doc["pa"]["ak"];
        an = constrain(an, 0, 180); // Ограничение угла 0–180
        ak = constrain(ak, 0, 180); // Ограничение угла 0–180
        bool servo1Changed = an != Servo1.read();
        bool servo2Changed = ak != Servo2.read();
        Servo1.write(an);
        Servo2.write(ak);

        // if (servo1Changed || servo2Changed)
        // {
        //     if (servo1Changed) Servo1.write(an);
        //     if (servo2Changed) Servo2.write(ak);
        //     //sendCommandAck("SAR");
        //     char logMsg[64];
        //     snprintf(logMsg, sizeof(logMsg), "Servo1 set to %d, Servo2 set to %d degrees", an, ak);
        //     //sendLogMessage(logMsg);
        // }
    }
    // else if (strcmp(co, "SAR2") == 0)
    // {
    //     int an = doc["pa"]["an"];
    //     an = constrain(an, 0, 180); // Ограничение угла 0–180
    //     if (an != Servo2.read())
    //     {
    //         Servo2.write(an);
    //         sendCommandAck("SSR2");
    //         char logMsg[32];
    //         snprintf(logMsg, sizeof(logMsg), "Servo2 set to %d degrees", an);
    //         //sendLogMessage(logMsg);
    //     }
    // }

    //control axis
    else if (strcmp(co, "SSY") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180); // Ограничение угла 0–180
        // if (an != Servo1.read())
        // {
            Servo1.write(an);
            //sendCommandAck("SSR");
            //sendLogMessage("SSY");
        //}
    }
    else if (strcmp(co, "SSX") == 0)
    {
        int an = doc["pa"]["an"];
        an = constrain(an, 0, 180); // Ограничение угла 0–180
        // if (an != Servo2.read())
        // {
            Servo2.write(an);
            //sendCommandAck("SSR2");
            //char logMsg[32];
            //snprintf(logMsg, sizeof(logMsg), "Servo2 set to %d degrees", an);

            // if(millis() - lastMillisAxisJoyX > 500){
            //     lastMillisAxisJoyX = millis();
            //     sendLogMessage("AxisX Joy");
            // }
            //sendLogMessage("SSX");
        //}
    }
    else if (strcmp(co, "SSA") == 0)
    {
        int an = doc["pa"]["an"];
        int SSA = Servo1.read();
        if(an > 0){
            if(SSA + an < 180) {
                Servo1.write(SSA + an);
            }
        }else{
            if(SSA - an > 70) {
                Servo1.write(SSA + an);
            }
        }
        //sendLogMessage("SSA");
    }
    else if (strcmp(co, "SSB") == 0)
    {
        int an = doc["pa"]["an"];
        int SSB = Servo2.read();
        if(an > 0){
            if(SSB + an < 180) {
                Servo2.write(SSB + an);
            }
        }else{
            if(SSB - an > 0) {
                Servo2.write(SSB + an);
            }
        }
        //sendLogMessage("SSB");
    }
    else if (strcmp(co, "GET_RELAYS") == 0)
    {
        char relayStatus[64];
        snprintf(relayStatus, sizeof(relayStatus), "Relay states: D0=%s, 3=%s",
                 digitalRead(button1) == LOW ? "on" : "off",
                 digitalRead(button2) == LOW ? "on" : "off");
        sendLogMessage(relayStatus);
        return;
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

        if (!pin || !state) {
            Serial.println("Ошибка: pin или state отсутствуют в JSON!");
            return;
        }

        if (strcmp(pin, "3") == 0)
        {
            digitalWrite(button1, strcmp(state, "on") == 0 ? LOW : HIGH);
            Serial.println("Relay 1 (3) set to: " + String(digitalRead(button1)));

        }
        else if (strcmp(pin, "D0") == 0)
        {
            digitalWrite(button2, strcmp(state, "on") == 0 ? LOW : HIGH);
            Serial.println("Relay 2 (D0) set to: " + String(digitalRead(button2)));
            Serial.println("Relay 2 (D0) set to: " + String(state));
        }

        // Формирование ответа
        StaticJsonDocument<256> ackDoc;
        ackDoc["ty"] = "ack";
        ackDoc["co"] = "RLY";
        ackDoc["de"] = de;
        JsonObject pa = ackDoc.createNestedObject("pa");
        pa["pin"] = pin;
        pa["state"] = state; // Используем запрошенное состояние вместо digitalRead
        String output;
        serializeJson(ackDoc, output);
        Serial.println("Отправка подтверждения на сервер: " + output);
        if (!client.send(output)) {
            Serial.println("Ошибка отправки подтверждения на сервер!");
        } else {
            Serial.println("Подтверждение успешно отправлено: " + output);
        }
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
    //Serial.end();
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

            if(digitalRead(button2) == HIGH) {
                if (millis() - lastAnalogReadTime > 300) {
                    lastAnalogReadTime = millis();
                    if(analogRead(analogPin) < 50  && millis() - lastMillisAlarm > 5000){
                        lastMillisAlarm = millis();
                        // Serial.println("ALARM TRUE 11111111111111111111111111111");
                        // Serial.print(analogRead(analogPin));
                        // Serial.print(" ");
                        // Serial.println(analogRead(button1));
                        sendLogMessage("ALARM TRUE");
                    }
                }
            }

            if (millis() - lastHeartbeatTime > 5000) {
                lastHeartbeatTime = millis();
                sendLogMessage("HBT");
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