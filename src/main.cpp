#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

const char* ssid = "Robolab124";
const char* password = "wifi123123123";
const char* websocket_server = "ws://192.168.0.151:8080";
const char* deviceId = "123"; // Должен совпадать с ID в клиенте

WebsocketsClient client;
unsigned long lastReconnectAttempt = 0;
unsigned long lastLogTime = 0;
unsigned long lastHeartbeatTime = 0;

void sendLogMessage(const char* message) {
  DynamicJsonDocument doc(256);
  doc["type"] = "log";
  doc["message"] = message;
  doc["deviceId"] = deviceId;
  String output;
  serializeJson(doc, output);
  client.send(output);
}

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Received: ");
  Serial.println(message.data());
  sendLogMessage("Received command from server");

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, message.data());

  if (!error) {
    const char* command = doc["command"];
    if (command) {
      if (strcmp(command, "forward") == 0) {
        Serial.println("Executing: FORWARD");
        sendLogMessage("Executing FORWARD command");
      } 
      else if (strcmp(command, "backward") == 0) {
        Serial.println("Executing: BACKWARD");
        sendLogMessage("Executing BACKWARD command");
      } 
      else if (strcmp(command, "servo") == 0) {
        int angle = doc["params"]["angle"];
        Serial.printf("Setting servo angle to: %d\n", angle);
        char logMsg[50];
        snprintf(logMsg, sizeof(logMsg), "Setting servo angle to %d", angle);
        sendLogMessage(logMsg);
      }
    }
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connected to server!");
    sendLogMessage("ESP connected to server");
    
    // Отправляем тип клиента
    DynamicJsonDocument typeDoc(128);
    typeDoc["type"] = "client_type";
    typeDoc["clientType"] = "esp";
    String typeOutput;
    serializeJson(typeDoc, typeOutput);
    client.send(typeOutput);
    
    // Идентификация устройства
    DynamicJsonDocument doc(128);
    doc["type"] = "identify";
    doc["deviceId"] = deviceId;
    String output;
    serializeJson(doc, output);
    client.send(output);

    // Уведомляем о подключении ESP
    DynamicJsonDocument statusDoc(128);
    statusDoc["type"] = "esp_status";
    statusDoc["status"] = "connected";
    statusDoc["deviceId"] = deviceId;
    String statusOutput;
    serializeJson(statusDoc, statusOutput);
    client.send(statusOutput);
  }
  else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connection closed");
    
    // Попытка уведомить сервер об отключении
    if (client.available()) {
      DynamicJsonDocument statusDoc(128);
      statusDoc["type"] = "esp_status";
      statusDoc["status"] = "disconnected";
      statusDoc["deviceId"] = deviceId;
      String statusOutput;
      serializeJson(statusDoc, statusOutput);
      client.send(statusOutput);
    }
  }
  else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  
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
        sendLogMessage("ESP reconnected to server");
      }
    }
  } else {
    client.poll();
    
    // Отправка heartbeat каждые 10 секунд
    if (millis() - lastHeartbeatTime > 5000) {
      lastHeartbeatTime = millis();
      sendLogMessage("ESP heartbeat - system normal");
    }
    
    // Пример отправки системных логов
    if (millis() - lastLogTime > 10000) {
      lastLogTime = millis();
      float voltage = analogRead(A0) * 3.3 / 1024.0;
      char voltageMsg[50];
      snprintf(voltageMsg, sizeof(voltageMsg), "System status - Voltage: %.2fV", voltage);
      sendLogMessage(voltageMsg);
    }
  }
}