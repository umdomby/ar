
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

const char* ssid = "Robolab124";
const char* password = "wifi123123123";
//const char* websocket_server = "ws://213.184.249.66:8085";
const char* websocket_server = "wss://ardu.site/ws";
const char* deviceId = "444";

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastLogTime = 0;
bool wasConnected = false;

void sendLogMessage(const char* message) {
  DynamicJsonDocument doc(256);
  doc["type"] = "log";
  doc["message"] = message;
  doc["deviceId"] = deviceId;
  String output;
  serializeJson(doc, output);
  client.send(output);
}

void sendCommandAck(const char* command) {
  DynamicJsonDocument doc(128);
  doc["type"] = "command_ack";
  doc["command"] = command;
  doc["deviceId"] = deviceId;
  String output;
  serializeJson(doc, output);
  client.send(output);
}

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Received: ");
  Serial.println(message.data());
  sendLogMessage("Command received");

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, message.data());

  if (!error) {
    const char* command = doc["command"];
    if (command) {
      if (strcmp(command, "forward") == 0) {
        Serial.println("Executing: FORWARD");
        sendLogMessage("Executing FORWARD");
        sendCommandAck("forward");
      } 
      else if (strcmp(command, "backward") == 0) {
        Serial.println("Executing: BACKWARD");
        sendLogMessage("Executing BACKWARD");
        sendCommandAck("backward");
      } 
      else if (strcmp(command, "servo") == 0) {
        int angle = doc["params"]["angle"];
        Serial.printf("Setting servo angle to: %d\n", angle);
        char logMsg[50];
        snprintf(logMsg, sizeof(logMsg), "Servo set to %d", angle);
        sendLogMessage(logMsg);
        sendCommandAck("servo");
      }
    }
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connected to server!");
    sendLogMessage("ESP connected to server");
    wasConnected = true;
    
    // Send client type
    DynamicJsonDocument typeDoc(128);
    typeDoc["type"] = "client_type";
    typeDoc["clientType"] = "esp";
    String typeOutput;
    serializeJson(typeDoc, typeOutput);
    client.send(typeOutput);
    
    // Identify device
    DynamicJsonDocument doc(128);
    doc["type"] = "identify";
    doc["deviceId"] = deviceId;
    String output;
    serializeJson(doc, output);
    client.send(output);

    // Send connection status
    DynamicJsonDocument statusDoc(128);
    statusDoc["type"] = "esp_status";
    statusDoc["status"] = "connected";
    statusDoc["deviceId"] = deviceId;
    String statusOutput;
    serializeJson(statusDoc, statusOutput);
    client.send(statusOutput);
    Serial.println("Sent ESP status: connected");
  }
  else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connection closed");
    if (wasConnected) {
      wasConnected = false;
      if (client.available()) {
        DynamicJsonDocument statusDoc(128);
        statusDoc["type"] = "esp_status";
        statusDoc["status"] = "disconnected";
        statusDoc["deviceId"] = deviceId;
        statusDoc["reason"] = "connection closed";
        String statusOutput;
        serializeJson(statusDoc, statusOutput);
        client.send(statusOutput);
      }
    }
  }
  else if (event == WebsocketsEvent::GotPing) {
    client.pong();
  }
}

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Setup WebSocket
  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  
  // Connect to server
  if (client.connect(websocket_server)) {
    Serial.println("WebSocket connected");
  } else {
    Serial.println("WebSocket connection failed!");
  }
}

void loop() {
  if (!client.available()) {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      if (client.connect(websocket_server)) {
        Serial.println("Reconnected to WebSocket");
        sendLogMessage("ESP reconnected");
      }
    }
  } else {
    client.poll();
    
    // Send heartbeat every 10 seconds
    if (millis() - lastHeartbeatTime > 1000) {
      lastHeartbeatTime = millis();
      sendLogMessage("Heartbeat - system OK");
    }
    
    // Send system info every 30 seconds
    if (millis() - lastLogTime > 10000) {
      lastLogTime = millis();
      float voltage = analogRead(A0) * 3.3 / 1024.0;
      char sysMsg[50];
      snprintf(sysMsg, sizeof(sysMsg), "System: %.2fV, %ddBm", 
               voltage, WiFi.RSSI());
      sendLogMessage(sysMsg);
    }
  }
}