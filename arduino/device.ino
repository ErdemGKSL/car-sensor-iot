/*
  Car Sensor IoT - Distance Sensor WebSocket Client
  Modified for ESP-01 module

  Hardware:
    - Arduino Uno
    - ESP-01 WiFi module (TX/RX connection)
    - Multiple HC-SR04 ultrasonic distance sensors
*/

#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>

// WiFi credentials and device identifier
char ssid[]     = "YOUR_SSID";
char password[] = "YOUR_PASSWORD";
const char* DEVICE_ID  = "DEVICE001";
const char* server     = "iot-server.erdemdev.tr";
const int port         = 80;

// Setup ESP-01 serial communication (Arduino pins)
// For ESP-01: Connect Arduino pin 10 to ESP-01 TX, Arduino pin 11 to ESP-01 RX
SoftwareSerial espSerial(10, 11); // RX, TX pins for ESP-01

// Sensor configuration structure
struct Sensor {
  const char* sensorId;
  int triggerPin;
  int echoPin;
};

// Define sensors
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", 4, 5 },
  { "S2", 6, 7 }
};

// Create WiFi client
WiFiEspClient client;
int status = WL_IDLE_STATUS;
bool isConnected = false;

// Timing variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 1 second
unsigned long lastReconnectAttempt = 0;

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

// Send the WebSocket handshake
bool connectWebSocket() {
  if (client.connect(server, port)) {
    Serial.println("Connected to server");
    
    // Send WebSocket handshake
    client.println("GET /arduino-ws HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Upgrade: websocket");
    client.println("Connection: Upgrade");
    client.println("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==");
    client.println("Sec-WebSocket-Version: 13");
    client.println();
    
    // Wait for response with lower timeout for ESP-01
    unsigned long timeout = millis() + 3000; // 3 second timeout
    while (client.available() == 0) {
      if (millis() > timeout) {
        Serial.println("Handshake timeout");
        client.stop();
        return false;
      }
      delay(50); // Shorter delay to avoid watchdog issues
    }
    
    // Read response
    String response = "";
    while (client.available()) {
      char c = client.read();
      response += c;
    }
    
    Serial.println("WebSocket response received");
    
    // Send registration message
    String regMessage = "{ \"type\": \"register\", \"device_id\": \"" + String(DEVICE_ID) + "\", \"sensors\": [";
    for (int i = 0; i < NUM_SENSORS; i++) {
      regMessage += "{ \"sensor_id\": \"" + String(sensors[i].sensorId) + "\" }";
      if (i < NUM_SENSORS - 1) {
        regMessage += ", ";
      }
    }
    regMessage += "] }";
    
    sendWebSocketMessage(regMessage);
    isConnected = true;
    return true;
  }
  
  Serial.println("Connection failed");
  return false;
}

// Create and send a WebSocket frame
void sendWebSocketMessage(String payload) {
  // Frame format for text message (non-masked for simplicity in this example)
  uint8_t frameHeader[10]; // Max header size
  size_t headerSize;
  size_t payloadLength = payload.length();
  
  // Basic frame header: FIN=1, Opcode=1 (text)
  frameHeader[0] = 0x81; // 10000001
  
  // Encode payload length
  if (payloadLength < 126) {
    frameHeader[1] = payloadLength;
    headerSize = 2;
  } else if (payloadLength < 65536) {
    frameHeader[1] = 126;
    frameHeader[2] = (payloadLength >> 8) & 0xFF;
    frameHeader[3] = payloadLength & 0xFF;
    headerSize = 4;
  } else {
    frameHeader[1] = 127;
    for (int i = 0; i < 8; i++) {
      frameHeader[2 + i] = (payloadLength >> (8 * (7 - i))) & 0xFF;
    }
    headerSize = 10;
  }
  
  // Send header
  for (size_t i = 0; i < headerSize; i++) {
    client.write(frameHeader[i]);
  }
  
  // Send payload
  client.print(payload);
  
  Serial.println("Sent message: " + payload);
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);  // Monitor serial at 9600
  espSerial.begin(9600); // ESP-01 at 9600 baud (more stable for ESP-01)
  
  delay(1000); // Allow ESP-01 to stabilize
  
  // Initialize ESP module with AT commands first
  espSerial.println("AT");
  delay(500);
  espSerial.println("AT+RST");
  delay(1000);
  
  // Initialize ESP8266 module
  WiFi.init(&espSerial);
  
  // Check if ESP8266 is present
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ESP-01 not responding");
    while (true); // Don't continue
  }
  
  // Initialize sensor pins
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].triggerPin, OUTPUT);
    pinMode(sensors[i].echoPin, INPUT);
  }
  
  // Connect to WiFi with retry mechanism
  Serial.print("Connecting to WiFi...");
  int attempts = 0;
  while (status != WL_CONNECTED && attempts < 10) {
    status = WiFi.begin(ssid, password);
    Serial.print(".");
    attempts++;
    delay(1000); // Longer delay for ESP-01
  }
  
  if (status != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi!");
    return;
  }
  
  Serial.println("\nWiFi connected");
  
  // Print IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  // Connect to WebSocket server
  Serial.println("Connecting to WebSocket server...");
  connectWebSocket();
}

void loop() {
  // Handle potential ESP-01 disconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect");
    status = WiFi.begin(ssid, password);
    delay(5000);
    return;
  }
  
  // Send sensor data at regular intervals
  if (isConnected && (millis() - lastSendTime >= sendInterval)) {
    lastSendTime = millis();
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      String dataMessage = "{ \"type\": \"data\", \"sensor_id\": \"" + String(sensors[i].sensorId) + "\", \"value\": " + String(distance) + " }";
      sendWebSocketMessage(dataMessage);
      delay(100); // Short delay between readings for ESP-01
    }
  }
  
  // Check for incoming data
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  
  // Check connection
  if (!client.connected() && isConnected) {
    Serial.println("Connection lost");
    isConnected = false;
  }
  
  // Reconnect if needed
  if (!isConnected && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      Serial.println("Attempting to reconnect...");
      connectWebSocket();
    }
  }
}
