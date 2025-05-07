/*
  Car Sensor IoT - Distance Sensor HTTP Client
  Adapted for ESP8266 NodeMCU

  Hardware:
    - NodeMCU ESP8266
    - Multiple HC-SR04 ultrasonic distance sensors
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// WiFi credentials and device identifier
const char* ssid = "Keenetic-0169";
const char* password = "GtXJup66";
const char* DEVICE_ID = "DEVICE001";
const char* server = "iot.erdemdev.tr";
const int port = 443;      // SSL port
const bool useSSL = true;  // Set to false to test with non-SSL connection

// API endpoints
const char* registerEndpoint = "/api/register";
const char* dataEndpoint = "/api/data";
const char* heartbeatEndpoint = "/api/heartbeat";

// Auth token received from server
String authToken = "";

// Sensor configuration structure
struct Sensor {
  const char* sensorId;
  int triggerPin;
  int echoPin;
};

// Define sensors - using NodeMCU pin mappings
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", D2, D1 },
  { "S2", D6, D5 },
};

ESP8266WiFiMulti WiFiMulti;
bool isRegistered = false;

// Timing variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000;  // 1 second
unsigned long lastHeartbeatTime = 0;
const unsigned long heartbeatInterval = 15000;  // 15 seconds

// Function to measure distance using the HC‑SR04 sensor
float measureDistance(const Sensor& sensor) {
  // Ensure trigger is low to start with
  digitalWrite(sensor.triggerPin, LOW);
  delayMicroseconds(2);

  // Trigger the sensor by setting it HIGH for 10 microseconds
  digitalWrite(sensor.triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sensor.triggerPin, LOW);

  // Read the echo pin; pulseIn returns duration in microseconds
  unsigned long duration = pulseIn(sensor.echoPin, HIGH, 100000);

  // Check for timeout
  if (duration == 0) {
    Serial.println("pulseIn() timeout!");
    return -1.0;  // Indicate an error
  }

  // Calculate distance in centimeters
  float distance = (duration / 2.0) / 29.1;
  return distance;
}

// Function to register device with server
bool registerDevice() {
  Serial.println("Registering device...");

  // Create JSON message using ArduinoJson
  DynamicJsonDocument doc(1024);
  doc["device_id"] = DEVICE_ID;

  JsonArray sensorsArray = doc.createNestedArray("sensors");
  for (int i = 0; i < NUM_SENSORS; i++) {
    JsonObject sensorObj = sensorsArray.createNestedObject();
    sensorObj["sensor_id"] = sensors[i].sensorId;
  }

  String registrationMessage;
  serializeJson(doc, registrationMessage);

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();  // Skip certificate validation for simplicity

  HTTPClient http;
  String url = String("https://") + server + registerEndpoint;

  if (!useSSL) {
    url = String("http://") + server + registerEndpoint;
  }

  Serial.println("Connecting to: " + url);

  if (http.begin(*client, url)) {
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(registrationMessage);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Registration response: " + payload);

      // Parse response
      DynamicJsonDocument responseDoc(1024);
      DeserializationError error = deserializeJson(responseDoc, payload);

      if (!error && responseDoc["success"]) {
        authToken = responseDoc["token"].as<String>();
        Serial.println("Device registered successfully!");
        Serial.println("Auth token: " + authToken);
        http.end();
        return true;
      } else {
        Serial.println("Failed to parse registration response");
      }
    } else {
      Serial.printf("Registration failed, error: %d %s\n", httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Unable to connect to server for registration");
  }

  return false;
}

// Function to send sensor data to the server
bool sendSensorData(const char* sensorId, float value) {
  if (authToken.length() == 0) {
    Serial.println("Cannot send data: No auth token");
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["token"] = authToken;
  doc["sensor_id"] = sensorId;
  doc["value"] = value;

  String dataMessage;
  serializeJson(doc, dataMessage);

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();  // Skip certificate validation for simplicity

  HTTPClient http;
  String url = String("https://") + server + dataEndpoint;

  if (!useSSL) {
    url = String("http://") + server + dataEndpoint;
  }

  if (http.begin(*client, url)) {
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(dataMessage);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument responseDoc(256);
      DeserializationError error = deserializeJson(responseDoc, payload);

      if (!error && responseDoc["success"]) {
        Serial.printf("Sent data for sensor %s: %.2f cm\n", sensorId, value);
        http.end();
        return true;
      } else {
        Serial.println("Failed to send data: " + payload);
      }
    } else if (httpCode == 401) {
      // Token expired or invalid
      Serial.println("Auth token rejected. Attempting to re-register...");
      isRegistered = false;
    } else {
      Serial.printf("Data send failed, error: %d %s\n", httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Unable to connect to server for sending data");
  }

  return false;
}

// Function to send heartbeat to the server
bool sendHeartbeat() {
  if (authToken.length() == 0) {
    Serial.println("Cannot send heartbeat: No auth token");
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["token"] = authToken;

  String heartbeatMessage;
  serializeJson(doc, heartbeatMessage);

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();  // Skip certificate validation for simplicity

  HTTPClient http;
  String url = String("https://") + server + heartbeatEndpoint;

  if (!useSSL) {
    url = String("http://") + server + heartbeatEndpoint;
  }

  if (http.begin(*client, url)) {
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(heartbeatMessage);

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Heartbeat sent successfully");
      http.end();
      return true;
    } else if (httpCode == 401) {
      // Token expired or invalid
      Serial.println("Auth token rejected. Attempting to re-register...");
      isRegistered = false;
    } else {
      Serial.printf("Heartbeat failed, error: %d %s\n", httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Unable to connect to server for heartbeat");
  }

  return false;
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);  // Higher baud rate for ESP8266

  Serial.println("\nCar Sensor IoT - NodeMCU HTTP Client");

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

  // Register the device
  if (registerDevice()) {
    isRegistered = true;
  } else {
    Serial.println("Initial registration failed. Will retry in loop()");
  }
}

void loop() {
  // Ensure WiFi is connected
  if (WiFiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    delay(1000);
    return;
  }

  // Try to register if not registered
  if (!isRegistered) {
    if (registerDevice()) {
      isRegistered = true;
      // Reset timers after successful registration
      lastSendTime = millis();
      lastHeartbeatTime = millis();
    } else {
      Serial.println("Registration failed. Retrying in 5 seconds...");
      delay(5000);
      return;
    }
  }

  // Send sensor data at regular intervals
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      if (distance < 0) {
        Serial.printf("Error reading sensor %s\n", sensors[i].sensorId);
      } else {
        if (!sendSensorData(sensors[i].sensorId, distance)) {
          Serial.printf("Failed to send data for sensor %s\n", sensors[i].sensorId);
        }
        Serial.printf("Distance from sensor %s: %.2f cm\n", sensors[i].sensorId, distance);  // Add this line
      }
      delay(50);  // Short delay between readings
    }
  }

  // Send heartbeat at regular intervals
  if (millis() - lastHeartbeatTime >= heartbeatInterval) {
    lastHeartbeatTime = millis();
    if (!sendHeartbeat()) {
      Serial.println("Failed to send heartbeat");
    }
  }

  yield();  // Allow the ESP8266 to handle background tasks
}
