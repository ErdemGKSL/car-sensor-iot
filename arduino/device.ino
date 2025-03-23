/*
  Car Sensor IoT - Distance Sensor WebSocket Client

  This sketch:
    1. Connects to a WiFi network via ESP8266 module
    2. Connects to a Websockets server
    3. Registers the device and sensors
    4. Periodically sends distance measurements from multiple HC-SR04 sensors
    5. Handles incoming messages

  Hardware:
    - Arduino board (Uno, Mega, etc.)
    - ESP8266 WiFi module (connected via serial)
    - Multiple HC-SR04 ultrasonic distance sensors
*/

#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>

// WiFi credentials and device identifier (set these as needed)
char ssid[]     = "YOUR_SSID";  // Changed from const char* to char[]
char password[] = "YOUR_PASSWORD";  // Changed from const char* to char[]
const char* DEVICE_ID  = "DEVICE001";
const char* server     = "iot-server.erdemdev.tr";
const int port         = 80;

// Setup ESP8266 serial communication
SoftwareSerial espSerial(2, 3); // RX, TX pins to communicate with ESP8266

// Sensor configuration structure
struct Sensor {
  const char* sensorId;  // Identifier for registration and data messages
  int triggerPin;        // Pin to trigger the sensor
  int echoPin;           // Pin to receive the echo
};

// Define the number of sensors and their settings (set at compile-time)
const int NUM_SENSORS = 2;
Sensor sensors[NUM_SENSORS] = {
  { "S1", 4, 5 },  // First sensor: identifier "S1" on Arduino pins 4 (trigger) and 5 (echo)
  { "S2", 6, 7 }   // Second sensor: identifier "S2" on Arduino pins 6 (trigger) and 7 (echo)
};

// Create WiFi client
WiFiEspClient client;
int status = WL_IDLE_STATUS;
bool isConnected = false;

// Timing variable for periodic data sending
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
    
    // Wait for server handshake response
    unsigned long timeout = millis() + 5000; // 5 second timeout
    while (client.available() == 0) {
      if (millis() > timeout) {
        Serial.println("Handshake timeout");
        client.stop();
        return false;
      }
      delay(100);
    }
    
    // Read and process the handshake response
    String response = "";
    while (client.available()) {
      char c = client.read();
      response += c;
    }
    
    Serial.println("WebSocket handshake response:");
    Serial.println(response);
    
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
  Serial.begin(115200);
  espSerial.begin(9600); // Set the baud rate for ESP8266 communication
  
  // Initialize ESP8266 module
  WiFi.init(&espSerial);
  
  // Check if ESP8266 is present
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ESP8266 not present");
    while (true); // Don't continue
  }
  
  // Initialize sensor pins for each sensor
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].triggerPin, OUTPUT);
    pinMode(sensors[i].echoPin, INPUT);
  }
  
  // Connect to WiFi network
  Serial.print("Connecting to WiFi...");
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, password);
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");
  
  // Print ESP8266 IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  // Connect to WebSocket server
  Serial.println("Connecting to WebSocket server...");
  connectWebSocket();
}

void loop() {
  // Send sensor data at regular intervals
  if (isConnected && (millis() - lastSendTime >= sendInterval)) {
    lastSendTime = millis();
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance = measureDistance(sensors[i]);
      String dataMessage = "{ \"type\": \"data\", \"sensor_id\": \"" + String(sensors[i].sensorId) + "\", \"value\": " + String(distance) + " }";
      sendWebSocketMessage(dataMessage);
    }
  }
  
  // Check for incoming data from server
  while (client.available()) {
    char c = client.read();
    Serial.write(c); // Print to Serial for debugging
    // In a real application, you'd handle WebSocket framing and process messages
  }
  
  // Check if the connection is still open
  if (!client.connected() && isConnected) {
    Serial.println("Connection lost");
    isConnected = false;
  }
  
  // Reconnect if needed
  if (!isConnected && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastReconnectAttempt > 5000) { // Try every 5 seconds
      lastReconnectAttempt = millis();
      Serial.println("Attempting to reconnect...");
      connectWebSocket();
    }
  }
}
