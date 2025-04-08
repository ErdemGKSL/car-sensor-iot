/*
  Car Sensor IoT - Distance Sensor WebSocket Client
  Modified for NodeMCU V3 ESP8266 ESP-12E

  Hardware:
    - NodeMCU ESP8266 (with built-in WiFi)
    - Multiple HC-SR04 ultrasonic distance sensors
*/

#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// WiFi credentials and device identifier
char ssid[]     = "YOUR_SSID";
char password[] = "YOUR_PASSWORD";
const char* DEVICE_ID  = "DEVICE001";
const char* server     = "iot-server.erdemdev.tr";
const int port         = 80;
const char* wsPath     = "/arduino-ws";

// Sensor configuration structure
struct Sensor {
  const char* sensorId;
  int triggerPin;
  int echoPin;
};

// Define sensors - using NodeMCU pin references
// NodeMCU pins: D0-D8 (GPIO 16, 5, 4, 0, 2, 14, 12, 13, 15)
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", D2, D1 },  // D2 = GPIO4, D1 = GPIO5
  { "S2", D6, D5 }   // D6 = GPIO12, D5 = GPIO14
};

// Create WebSocket client
WebSocketsClient webSocket;
bool isConnected = false;

// Timing variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 1 second

// Function to measure distance using the HC‑SR04 sensor
float measureDistance(const Sensor &sensor) {
  // Ensure trigger is low to start with
  digitalWrite(sensor.triggerPin, LOW);
  delayMicroseconds(2);
  
  // Trigger the sensor by setting it HIGH for 10 microseconds
  digitalWrite(sensor.triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sensor.triggerPin, LOW);
  
  // Read the echo pin; pulseIn returns duration in microseconds
  unsigned long duration = pulseIn(sensor.echoPin, HIGH);
  
  // Calculate distance in centimeters
  float distance = (duration / 2.0) / 29.1;
  return distance;
}

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      isConnected = false;
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      isConnected = true;
      
      // Send registration message when connected
      DynamicJsonDocument doc(512);
      doc["type"] = "register";
      doc["device_id"] = DEVICE_ID;
      
      JsonArray sensorsArray = doc.createNestedArray("sensors");
      for (int i = 0; i < NUM_SENSORS; i++) {
        JsonObject sensorObj = sensorsArray.createNestedObject();
        sensorObj["sensor_id"] = sensors[i].sensorId;
      }
      
      String regMessage;
      serializeJson(doc, regMessage);
      webSocket.sendTXT(regMessage);
      Serial.println("Registration message sent: " + regMessage);
      break;
    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);
      // Handle incoming messages if needed
      break;
    case WStype_ERROR:
      Serial.println("WebSocket error");
      break;
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);  // Higher baud rate for ESP8266
  delay(1000);
  
  Serial.println("\nCar Sensor IoT - NodeMCU Edition");
  
  // Initialize sensor pins
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].triggerPin, OUTPUT);
    pinMode(sensors[i].echoPin, INPUT);
  }
  
  // Connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  
  // Print IP address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Configure and connect WebSocket client
  webSocket.begin(server, port, wsPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  Serial.println("WebSocket client started");
}

void loop() {
  // Keep the WebSocket connection alive
  webSocket.loop();
  
  // Send sensor data at regular intervals
  if (isConnected && (millis() - lastSendTime >= sendInterval)) {
    lastSendTime = millis();
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      
      DynamicJsonDocument doc(256);
      doc["type"] = "data";
      doc["sensor_id"] = sensors[i].sensorId;
      doc["value"] = distance;
      
      String dataMessage;
      serializeJson(doc, dataMessage);
      webSocket.sendTXT(dataMessage);
      
      Serial.println("Sent message: " + dataMessage);
      delay(50); // Small delay between readings
    }
  }
}
