#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

// const char* ws_server = "ws://192.168.1.139:8080/ws"; // Change to your server's IP
const char* ws_server = "ws://smartgrid-app.xyz:8080/ws"; // Change to your server's IP
using namespace websockets;

WebsocketsClient client;

const int signalInPin = 14;   // GPIO4 for detecting the signal
const int ledPin = 27;  // GPIO for built-in LED

const int stepPin = 5; 
const int dirPin = 18; 
const int enPin = 17;

const int stepPin2 = 4; 
const int dirPin2 = 16; 
const int enPin2 = 17;

unsigned long lastReconnectAttempt = 0;

volatile unsigned long lastEdgeTime = 0; // Time of the last detected rising edge
volatile unsigned long currentEdgeTime = 0; // Time of the current detected rising edge
volatile float frequency = 0.0; // Calculated frequency in Hz

// Frequency range (acceptable range for 60 Hz)
const float lowerBound = 59.000;
const float upperBound = 61.000;

// Variables for non-blocking LED blinking
bool ledState = false;
unsigned long lastBlinkTime = 0; // Tracks the last time the LED state changed
const unsigned long blinkInterval = 100; // Blink interval in milliseconds

void IRAM_ATTR onEdge() {
    currentEdgeTime = micros(); // Get the current time
    unsigned long interval = currentEdgeTime - lastEdgeTime; // Calculate time difference
    
    if (interval > 100 && interval < 500000) { // Ignore noise and large intervals
        lastEdgeTime = currentEdgeTime; // Update the last edge time
        frequency = 1000000.0 / interval; // Calculate frequency in Hz
    } else {
        frequency = 0; // Treat invalid intervals as no signal
    } 
}

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Received: ");
    Serial.println(message.data());

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message.data());

    if (error) {
        Serial.println("JSON Parse Error!");
        return;
    }

    // Extract command and optional breakerId
    String command = doc["command"].as<String>();
    int breakerId = doc.containsKey("breakerId") ? doc["breakerId"].as<int>() : -1;

    String responseMessage;
    bool success = false;

    if (command == "pingDevice") {
        responseMessage = "{\"message\": \"ACK\"}";
        success = true;
    } 
    else if (command == "flashLED") {
        for (int i = 0; i < 3; i++) {
            digitalWrite(ledPin, HIGH);
            delay(75);
            digitalWrite(ledPin, LOW);
            delay(75);
        }
        responseMessage = "{\"message\": \"Flashed LED 3 times\"}";
        success = true;
    } 
    else if (command == "toggleBreaker" && breakerId != -1) {
        responseMessage = "{\"message\": \"Breaker " + String(breakerId) + " has been toggled\"}";
        success = true;
    } 
    else {
        responseMessage = "{\"error\": \"Invalid command\"}";
    }

    // Send response if the command was valid
    if (success) {
        bool sendResult = client.send(responseMessage);
        if (sendResult) {
            Serial.println("Acknowledgment sent successfully!");
        } else {
            Serial.println("Failed to send acknowledgment!");
        }
    }
}

void connectWebSocket() {
    Serial.println("Connecting to WebSocket...");
    if (client.connect(ws_server)) {
        Serial.println("Connected to WebSocket server");
        client.send(WiFi.macAddress());  // Send MAC address for server identification
    } else {
        Serial.println("WebSocket connection failed!");
    }
}

void setup() {
    Serial.begin(115200);
    
    // Pin configuration
    pinMode(signalInPin, INPUT_PULLDOWN); // GPIO4 as input with pull-down resistor
    pinMode(ledPin, OUTPUT);             // LED for visual feedback
    
    pinMode(stepPin,OUTPUT);            
    pinMode(dirPin,OUTPUT);
    pinMode(enPin,OUTPUT);
    pinMode(stepPin2,OUTPUT); 
    pinMode(dirPin2,OUTPUT);
    pinMode(enPin2,OUTPUT);

    digitalWrite(enPin,LOW);
    digitalWrite(enPin2,LOW);
    digitalWrite(ledPin, LOW); // Ensure the LED is off 

    // Attach interrupt for frequency detection on GPIO4
    attachInterrupt(digitalPinToInterrupt(signalInPin), onEdge, RISING);

    Serial.println("Setup complete. Ready to detect frequency from waveform generator...");

    // Initialize WiFiManager
    WiFiManager wifiManager;

    // Uncomment this to reset saved Wi-Fi credentials
    // wifiManager.resetSettings(); 

    // Automatically connect to last used Wi-Fi or open a captive portal if not connected
    if (!wifiManager.autoConnect("ESP32-Config", "password")) {
        Serial.println("Failed to connect to Wi-Fi and hit timeout");
        ESP.restart();
    }

    Serial.println("Connected to Wi-Fi!");

    client.onMessage(onMessageCallback);
    connectWebSocket();
}

void loop() {
    // Poll WebSocket to receive messages
    if (client.available()) {
        client.poll();
    }

    // Reconnect WebSocket if disconnected
    if (!client.available() && (millis() - lastReconnectAttempt > 5000)) {
        Serial.println("Reconnecting WebSocket...");
        lastReconnectAttempt = millis();
        connectWebSocket();
    } 

    unsigned long currentTime = millis();

    // Check if frequency is out of range or no signal is detected
    if (frequency < lowerBound || frequency > upperBound || frequency == 0) {
        // Non-blocking LED blinking
        if (currentTime - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = currentTime; // Update the last blink time
            ledState = !ledState;        // Toggle LED state
            digitalWrite(ledPin, ledState); // Set the LED state
        }
    } else {
        // Keep the LED off if the frequency is within range
        digitalWrite(ledPin, LOW);
    }

    // Print debug information less frequently (every 2 seconds)
    static unsigned long lastPrintTime = 0;
    if (currentTime - lastPrintTime >= 2000) { // Adjust interval here
        lastPrintTime = currentTime;

        if (frequency > 0.0) {
            Serial.print("Measured Frequency: ");
            Serial.print(frequency);
            Serial.println(" Hz");
        } else {
            Serial.println("No signal detected (Frequency = 0 Hz).");
        }
    }
}
