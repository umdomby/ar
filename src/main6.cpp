// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <ArduinoWebsockets.h>
// #include <ArduinoJson.h>

// using namespace websockets;

// const char* ssid = "Robolab124";
// const char* password = "wifi123123123";
// const char* websocket_server = "wss://ardu.site/ws";
// const char* deviceId = "123";

// WebsocketsClient client;
// unsigned long lastReconnectAttempt = 0;
// unsigned long lastHeartbeatTime = 0;
// unsigned long lastLogTime = 0;
// unsigned long lastHeartbeat2Time = 0; // Время последнего Heartbeat2
// bool wasConnected = false;

// // Motor A
// #define enA D6
// #define in1 D2
// #define in2 D7

// // Motor B
// #define enB D5
// #define in3 D3
// #define in4 D8

// void sendLogMessage(const char* message) {
//     DynamicJsonDocument doc(256);
//     doc["type"] = "log";
//     doc["message"] = message;
//     doc["deviceId"] = deviceId;
//     String output;
//     serializeJson(doc, output);
//     client.send(output);
// }

// void sendCommandAck(const char* command) {
//     DynamicJsonDocument doc(128);
//     doc["type"] = "command_ack";
//     doc["command"] = command;
//     doc["deviceId"] = deviceId;
//     String output;
//     serializeJson(doc, output);
//     client.send(output);
// }

// void stopMotors() {
//     analogWrite(enA, 0);
//     analogWrite(enB, 0);
//     digitalWrite(in1, LOW);
//     digitalWrite(in2, LOW);
//     digitalWrite(in3, LOW);
//     digitalWrite(in4, LOW);
//     sendLogMessage("Motors stopped due to lost connection");
// }

// void onMessageCallback(WebsocketsMessage message) {
//   Serial.print("Received: ");
//   Serial.println(message.data());
//   sendLogMessage("Command received");

//   DynamicJsonDocument doc(256);
//   DeserializationError error = deserializeJson(doc, message.data());

//   if (error) {
//       Serial.print("JSON parsing failed: ");
//       Serial.println(error.c_str());
//       return;
//   }

//   const char* command = doc["command"];
//   if (!command) return;

//   // Обработка команд управления моторами
//   if (strcmp(command, "motor_a_forward") == 0) {
//       Serial.println("Motor A: FORWARD");
//       digitalWrite(in1, HIGH);
//       digitalWrite(in2, LOW);
//   }
//   else if (strcmp(command, "motor_a_backward") == 0) {
//       Serial.println("Motor A: BACKWARD");
//       digitalWrite(in1, LOW);
//       digitalWrite(in2, HIGH);
//   }
//   else if (strcmp(command, "motor_b_forward") == 0) {
//       Serial.println("Motor B: FORWARD");
//       digitalWrite(in3, HIGH);
//       digitalWrite(in4, LOW);
//   }
//   else if (strcmp(command, "motor_b_backward") == 0) {
//       Serial.println("Motor B: BACKWARD");
//       digitalWrite(in3, LOW);
//       digitalWrite(in4, HIGH);
//   }
//   else if (strcmp(command, "set_speed") == 0) {
//       const char* motor = doc["params"]["motor"];
//       int speed = doc["params"]["speed"];
      
//       if (strcmp(motor, "A") == 0) {
//           analogWrite(enA, speed);
//           Serial.printf("Motor A speed: %d\n", speed);
//       } 
//       else if (strcmp(motor, "B") == 0) {
//           analogWrite(enB, speed);
//           Serial.printf("Motor B speed: %d\n", speed);
//       }
//   }
//   else if (strcmp(command, "stop") == 0) {
//       Serial.println("STOP all motors");
//       stopMotors();
//   }
//   else if (strcmp(command, "heartbeat2") == 0) {
//       lastHeartbeat2Time = millis();
//       return;  // Не логируем heartbeat для уменьшения спама
//   }

//   // Отправка подтверждения для всех команд кроме heartbeat
//   if (strcmp(command, "heartbeat2") != 0) {
//       sendCommandAck(command);
//       char logMsg[60];
//       snprintf(logMsg, sizeof(logMsg), "Executed: %s", command);
//       sendLogMessage(logMsg);
//   }
// }

// void onEventsCallback(WebsocketsEvent event, String data) {
//     if (event == WebsocketsEvent::ConnectionOpened) {
//         Serial.println("Connected to server!");
//         sendLogMessage("ESP connected to server");
//         wasConnected = true;

//         // Send client type
//         DynamicJsonDocument typeDoc(128);
//         typeDoc["type"] = "client_type";
//         typeDoc["clientType"] = "esp";
//         String typeOutput;
//         serializeJson(typeDoc, typeOutput);
//         client.send(typeOutput);

//         // Identify device
//         DynamicJsonDocument doc(128);
//         doc["type"] = "identify";
//         doc["deviceId"] = deviceId;
//         String output;
//         serializeJson(doc, output);
//         client.send(output);

//         // Send connection status
//         DynamicJsonDocument statusDoc(128);
//         statusDoc["type"] = "esp_status";
//         statusDoc["status"] = "connected";
//         statusDoc["deviceId"] = deviceId;
//         String statusOutput;
//         serializeJson(statusDoc, statusOutput);
//         client.send(statusOutput);
//         Serial.println("Sent ESP status: connected");
//     }
//     else if (event == WebsocketsEvent::ConnectionClosed) {
//         Serial.println("Connection closed");
//         if (wasConnected) {
//             wasConnected = false;
//             stopMotors(); // Останавливаем двигатели при потере соединения
//             if (client.available()) {
//                 DynamicJsonDocument statusDoc(128);
//                 statusDoc["type"] = "esp_status";
//                 statusDoc["status"] = "disconnected";
//                 statusDoc["deviceId"] = deviceId;
//                 statusDoc["reason"] = "connection closed";
//                 String statusOutput;
//                 serializeJson(statusDoc, statusOutput);
//                 client.send(statusOutput);
//             }
//         }
//     }
//     else if (event == WebsocketsEvent::GotPing) {
//         client.pong();
//     }
// }

// void setup() {
//     Serial.begin(115200);

//     // Connect to WiFi
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nWiFi connected");
//     Serial.print("IP: ");
//     Serial.println(WiFi.localIP());

//     // Setup WebSocket
//     client.onMessage(onMessageCallback);
//     client.onEvent(onEventsCallback);

//     // Connect to server
//     if (client.connect(websocket_server)) {
//         Serial.println("WebSocket connected");
//     } else {
//         Serial.println("WebSocket connection failed!");
//     }

//     // Initialize motor pins
//     pinMode(enA, OUTPUT);
//     pinMode(enB, OUTPUT);
//     pinMode(in1, OUTPUT);
//     pinMode(in2, OUTPUT);
//     pinMode(in3, OUTPUT);
//     pinMode(in4, OUTPUT);
//     digitalWrite(in1, LOW);
//     digitalWrite(in2, LOW);
//     digitalWrite(in3, LOW);
//     digitalWrite(in4, LOW);
// }

// void loop() {
//   if (!client.available()) {
//       if (millis() - lastReconnectAttempt > 5000) {
//           lastReconnectAttempt = millis();
//           if (client.connect(websocket_server)) {
//               Serial.println("Reconnected to WebSocket");
//               sendLogMessage("ESP reconnected");
//           }
//       }
//   } else {
//       client.poll();

//       // Send heartbeat every 10 seconds
//       if (millis() - lastHeartbeatTime > 10000) {
//           lastHeartbeatTime = millis();
//           sendLogMessage("Heartbeat - system OK");
//       }

//       // Check for Heartbeat2 timeout
//       if (millis() - lastHeartbeat2Time > 2000) {
//           stopMotors(); // Останавливаем двигатели, если Heartbeat2 не получен в течение 2 секунд
//       }

//       // Send system info every 30 seconds
//       if (millis() - lastLogTime > 30000) {
//           lastLogTime = millis();
//           float voltage = analogRead(A0) * 3.3 / 1024.0;
//           char sysMsg[50];
//           snprintf(sysMsg, sizeof(sysMsg), "System: %.2fV, %ddBm",
//               voltage, WiFi.RSSI());
//           sendLogMessage(sysMsg);
//       }
//   }
// }

