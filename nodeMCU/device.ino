/*
  Car Sensor IoT - Distance Sensor WebSocket Client
  Adapted for ESP8266 NodeMCU

  Hardware:
    - NodeMCU ESP8266
    - Multiple HC-SR04 ultrasonic distance sensors
*/
#define _WEBSOCKETS_LOGLEVEL_     2

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>
#include <Hash.h>


// WiFi credentials and device identifier
const char* ssid = "Keenetic-0169";
const char* password = "GtXJup66";
const char* DEVICE_ID = "DEVICE001";
const char* server = "iot-server.erdemdev.tr";  // Removed wss:// prefix
const int port = 443; // SSL port
const bool useSSL = true; // Set to false to test with non-SSL connection

// Sensor configuration structure
struct Sensor {
  const char* sensorId;
  int triggerPin;
  int echoPin;
};

// Define sensors - using NodeMCU pin mappings
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", D5, D6 },  // Added proper sensor ID
  { "S2", D1, D2 }   // Added second sensor with different pins
};

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
bool isConnected = false;

// Connection monitoring
unsigned long connectionStartTime = 0;
const unsigned long CONNECTION_TIMEOUT = 30000; // 30 seconds timeout
bool connectionTimedOut = false;

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

void webSocketEvent(const WStype_t& type, uint8_t * payload, const size_t& length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (isConnected) {
        Serial.println("[WSc] Disconnected!");
        isConnected = false;
      }
      break;

    case WStype_CONNECTED:
    {
      Serial.print("[WSc] Connected to url: ");
      Serial.println((char *) payload);
      
      isConnected = true;
      connectionTimedOut = false;
      
      // Send registration message using ArduinoJson
      DynamicJsonDocument doc(1024);
      doc["type"] = "register";
      doc["device_id"] = DEVICE_ID;
      
      JsonArray sensorsArray = doc.createNestedArray("sensors");
      
      for (int i = 0; i < NUM_SENSORS; i++) {
        JsonObject sensorObj = sensorsArray.createNestedObject();
        sensorObj["sensor_id"] = sensors[i].sensorId;
      }
      
      String registrationMessage;
      serializeJson(doc, registrationMessage);
      webSocket.sendTXT(registrationMessage);
      
      Serial.println("Registration message sent: " + registrationMessage);
      break;
    }

    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      break;

    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;

    case WStype_PING:
      // pong will be sent automatically
      Serial.printf("[WSc] get ping\n");
      break;

    case WStype_PONG:
      // answer to a ping we send
      Serial.printf("[WSc] get pong\n");
      break;

    case WStype_ERROR:
      Serial.printf("[WSc] ERROR: %s\n", payload);
      break;

    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;

    default:
      break;
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);  // Higher baud rate for ESP8266
  
  Serial.println("\nCar Sensor IoT - NodeMCU WebSocket Client");
  
  // Initialize sensor pins
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].triggerPin, OUTPUT);
    pinMode(sensors[i].echoPin, INPUT);
  }
  
  // Connect to WiFi
  WiFiMulti.addAP(ssid, password);
  
  Serial.print("Connecting to WiFi...");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  
  Serial.println("\nWiFi connected");
  
  // Print IP address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Display network information
  Serial.print("Connected to SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  // Connect to WebSocket server
  Serial.printf("Connecting to WebSocket Server @ %s:%d%s\n", server, port, useSSL ? " (SSL)" : "");
  
  if (useSSL) {
    webSocket.beginSSL(server, port, "/arduino-ws");
    Serial.println("SSL connection attempt started");
  } else {
    webSocket.begin(server, port, "/arduino-ws");
    Serial.println("Non-SSL connection attempt started");
  }
  
  webSocket.onEvent(webSocketEvent);
  
  // Set reconnection interval (5 seconds)
  webSocket.setReconnectInterval(5000);
  
  // Enable heartbeat
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  // Start connection timer
  connectionStartTime = millis();
}

void loop() {
  webSocket.loop();
  
  // Check for connection timeout
  if (!isConnected && !connectionTimedOut && (millis() - connectionStartTime > CONNECTION_TIMEOUT)) {
    connectionTimedOut = true;
    Serial.println("WARNING: WebSocket connection timed out!");
    Serial.println("Potential issues:");
    Serial.println("1. Server may be unreachable");
    Serial.println("2. SSL certificate issues");
    Serial.println("3. Wrong server address/port");
    Serial.println("4. WebSocket path may be incorrect");
    Serial.println("Try setting 'useSSL = false' for debugging");
    
    // Optional: Restart ESP after timeout
    // ESP.restart();
  }
  
  // Send sensor data only if properly connected
  if (isConnected && (millis() - lastSendTime >= sendInterval)) {
    lastSendTime = millis();
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      
      // Create JSON message using ArduinoJson
      DynamicJsonDocument doc(256);
      doc["type"] = "data";
      doc["sensor_id"] = sensors[i].sensorId;
      doc["value"] = distance;
      
      String dataMessage;
      serializeJson(doc, dataMessage);
      
      webSocket.sendTXT(dataMessage);
      Serial.printf("Sent data: %s\n", dataMessage.c_str());  // Added logging
      delay(50); // Short delay between readings
    }
  }
  
  yield(); // Allow the ESP8266 to handle background tasks
}
