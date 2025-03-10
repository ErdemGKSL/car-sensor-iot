#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

// WiFi credentials and device identifier (set these as needed)
const char* ssid       = "YOUR_SSID";
const char* password   = "YOUR_PASSWORD";
const char* DEVICE_ID  = "DEVICE001";

// Sensor configuration structure
struct Sensor {
  const char* sensorId;  // Identifier for registration and data messages
  int triggerPin;        // Pin to trigger the sensor
  int echoPin;           // Pin to receive the echo
};

// Define the number of sensors and their settings (set at compile-time)
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", D1, D2 },  // First sensor: identifier "S1" on pins D1 (trigger) and D2 (echo)
  { "S2", D3, D4 }   // Second sensor: identifier "S2" on pins D3 (trigger) and D4 (echo)
};

// Create an instance of the WebSocket client
WebSocketsClient webSocket;

// Timing variable for periodic data sending
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
  
  // Calculate distance in centimeters:
  // (duration / 2) gives the one-way time; dividing by 29.1 approximates cm based on the speed of sound.
  float distance = (duration / 2.0) / 29.1;
  return distance;
}

// Callback function for WebSocket events
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.println("WebSocket connected");
      // Build a registration JSON message with device id and sensor slots
      String regMessage = "{ \"type\": \"register\", \"device_id\": \"" + String(DEVICE_ID) + "\", \"sensors\": [";
      for (int i = 0; i < NUM_SENSORS; i++) {
        regMessage += "{ \"sensor_id\": \"" + String(sensors[i].sensorId) + "\" }";
        if (i < NUM_SENSORS - 1) {
          regMessage += ", ";
        }
      }
      regMessage += "] }";
      Serial.println("Sending registration: " + regMessage);
      webSocket.sendTXT(regMessage);
      break;
    }
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      break;
    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);
      break;
    case WStype_BIN:
      Serial.println("Received binary data");
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize sensor pins for each sensor in the array
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].triggerPin, OUTPUT);
    pinMode(sensors[i].echoPin, INPUT);
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize the WebSocket connection
  // Replace "echo.websocket.org" and port if you have your own server
  webSocket.begin("iot-server.erdemdev.tr", 80, "/arduino-ws");
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  // Handle WebSocket events
  webSocket.loop();
  
  // Every second, read each sensor and send its data via WebSocket
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      // Create a JSON message containing sensor id and measured value
      String dataMessage = "{ \"type\": \"data\", \"sensor_id\": \"" + String(sensors[i].sensorId) + "\", \"value\": " + String(distance) + " }";
      Serial.println("Sending data: " + dataMessage);
      webSocket.sendTXT(dataMessage);
    }
  }
}
