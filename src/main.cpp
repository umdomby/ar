#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

const char* ssid = "Robolab124";
const char* password = "wifi123123123";
const char* websocket_server = "ws://213.184.249.66:8080";

WebsocketsClient client;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 15000; // 15 секунд

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Received: ");
    Serial.println(message.data());

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message.data());

    if (!error) {
        const char* command = doc["command"];
        if (command) {
            if (strcmp(command, "forward") == 0) {
                Serial.println("Moving forward");
            } else if (strcmp(command, "backward") == 0) {
                Serial.println("Moving backward");
            } else if (strcmp(command, "servo") == 0) {
                int angle = doc["params"]["angle"];
                Serial.print("Setting servo angle to ");
                Serial.println(angle);
            }
        } else {
            Serial.println("Command not found in JSON");
        }
    } else {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connection Opened");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connection Closed");
    } else if (event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if (event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi");

    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);

    client.connect(websocket_server);
}

void loop() {
    client.poll();

    if (millis() - lastPingTime >= pingInterval) {
        client.ping();
        lastPingTime = millis();
    }
}
