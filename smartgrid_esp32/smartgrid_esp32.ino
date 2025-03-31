#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// WebSocket Configuration
constexpr char WS_SERVER[] = "ws://smartgrid-app.xyz:8080/ws"; 
WebsocketsClient client;

// Frequency Sensing Pin Definitions
constexpr int SIGNAL_IN_PIN = 12;
constexpr int LED_PIN = 27;

// Stepper Motor 1
constexpr int STEP_PIN_1 = 5;
constexpr int DIR_PIN_1 = 18;
constexpr int EN_PIN_1 = 17;

// Stepper Motor 2
constexpr int STEP_PIN_2 = 4;
constexpr int DIR_PIN_2 = 16;
constexpr int EN_PIN_2 = 17;

// Frequency & LED Control
constexpr float LOWER_FREQ_BOUND = 59.0f;
constexpr float UPPER_FREQ_BOUND = 61.0f;
constexpr unsigned long BLINK_INTERVAL = 100;

// Timing Variables
unsigned long lastReconnectAttempt = 0;
volatile unsigned long lastEdgeTime = 0;
volatile float measuredFrequency = 0.0f;
int FreqqyPacketFlag;

// LED Blinking State
bool ledState = false;
unsigned long lastBlinkTime = 0;
unsigned long lastPrintTime = 0;

WiFiManager wifiManager;

// Interrupt Service Routine (ISR) for Frequency Detection
void IRAM_ATTR onEdge() {
    unsigned long currentEdgeTime = micros();
    unsigned long interval = currentEdgeTime - lastEdgeTime;

    if (interval > 100 && interval < 500000) { 
        lastEdgeTime = currentEdgeTime;
        measuredFrequency = 1'000'000.0f / interval;
    } else {
        measuredFrequency = 0.0f;
    }
}

// Function to Send WebSocket Message
void sendWebSocketMessage(const char* command, int breakerId = -1, bool breakerState = -1) {
    StaticJsonDocument<128> doc;
    doc["command"] = command;
    doc["mac_addr"] = WiFi.macAddress();

    if (breakerId != -1) doc["breakerId"] = breakerId;
    if (breakerState != -1) doc["breakerState"] = breakerState;

    char messageBuffer[128];
    serializeJson(doc, messageBuffer);

    Serial.print("Sending Back: ");
    Serial.println(messageBuffer);

    client.send(messageBuffer);
}

// Function to Send Frequency Data to Server
void sendFrequencyUpdate() {
    if (millis() - lastPrintTime >= 2000) { // Every 2 seconds
        lastPrintTime = millis();
        
        if (FreqqyPacketFlag == 30){
            FreqqyPacketFlag = 0;
            StaticJsonDocument<128> doc;
            doc["mac_addr"] = WiFi.macAddress();
            doc["command"] = "frequencyUpdate";
            doc["frequency"] = measuredFrequency;

            char messageBuffer[128];
            serializeJson(doc, messageBuffer);
            client.send(messageBuffer);
        }

        FreqqyPacketFlag++;
        Serial.printf("Sent Frequency: %.3f Hz\n", measuredFrequency);
    }
}

// Handle LED Blinking Based on Frequency
void handleLEDBlinking() {
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;
    unsigned long currentTime = millis();

    if (measuredFrequency < 50 || measuredFrequency > 500 || measuredFrequency == 0.0f) {
        if (currentTime - lastBlinkTime >= 500) { // Blink every 500ms
            lastBlinkTime = currentTime;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
        }
    } else {
        digitalWrite(LED_PIN, LOW);
    }
}

// Handle Incoming WebSocket Messages
void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Received: ");
    Serial.println(message.data());

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, message.data())) {
        Serial.println("JSON Parse Error!");
        return;
    }

    const char* command = doc["command"];
    int breakerId = doc["breakerId"] | -1;
    bool breakerState = doc["breakerState"] | false;

    if (strcmp(command, "pingDevice") == 0) {
        sendWebSocketMessage("ACK");
    } 
    else if (strcmp(command, "flashLED") == 0) {
        flashLED(3);
        sendWebSocketMessage("ACK");
    } 
    else if (strcmp(command, "toggleBreaker") == 0 && breakerId != -1) {
        sendWebSocketMessage(command, breakerId, !breakerState);
    } 
    else {
        Serial.println("Unknown Command Received.");
    }
}

// Flash LED a Given Number of Times
void flashLED(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(75);
        digitalWrite(LED_PIN, LOW);
        delay(75);
    }
}

// Connect to WebSocket Server
void connectWebSocket() {
    Serial.println("Connecting to WebSocket...");
    if (client.connect(WS_SERVER)) {
        Serial.println("Connected to WebSocket server");
        client.send(WiFi.macAddress());
    } else {
        Serial.println("WebSocket connection failed!");
    }
}

// Handle WebSocket Connection
void handleWebSocket() {
    if (client.available()) {
        client.poll();
    } else if (millis() - lastReconnectAttempt > 5000) {
        Serial.println("Reconnecting WebSocket...");
        lastReconnectAttempt = millis();
        connectWebSocket();
    }
}

// Setup Function
void setup() {
    Serial.begin(115200);

    pinMode(SIGNAL_IN_PIN, INPUT_PULLDOWN);
    pinMode(LED_PIN, OUTPUT);

    pinMode(STEP_PIN_1, OUTPUT);
    pinMode(DIR_PIN_1, OUTPUT);
    pinMode(EN_PIN_1, OUTPUT);

    pinMode(STEP_PIN_2, OUTPUT);
    pinMode(DIR_PIN_2, OUTPUT);
    pinMode(EN_PIN_2, OUTPUT);

    digitalWrite(EN_PIN_1, LOW);
    digitalWrite(EN_PIN_2, LOW);
    digitalWrite(LED_PIN, LOW);

    attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN), onEdge, RISING);
    Serial.println("Setup complete. Ready to detect frequency...");

    // Uncomment this to reset saved Wi-Fi credentials
    // wifiManager.resetSettings(); 

    if (!wifiManager.autoConnect("ESP32-Config", "password")) {
        Serial.println("Wi-Fi connection failed. Restarting...");
        ESP.restart();
    }

    Serial.println("Connected to Wi-Fi!");
    client.onMessage(onMessageCallback);
    connectWebSocket();
}

// Main Loop
void loop() {
    handleWebSocket();
    handleLEDBlinking();
    sendFrequencyUpdate();
}