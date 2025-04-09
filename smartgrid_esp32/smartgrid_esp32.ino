#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <RunningMedian.h>

using namespace websockets;

// --- Constants ---
constexpr char WS_SERVER[] = "ws://smartgrid-app.xyz:8080/ws";
constexpr int SIGNAL_IN_PIN = 25;

// Stepper Pins
const int stepPinY = 4, dirPinY = 16, enPinY = 17;
const int stepPinX = 14, dirPinX = 18, enPinX = 19;

// Limit Switch Pins
const uint8_t limitPinY = 27, limitPinX = 26;

// Frequency & LED Control
constexpr float LOWER_FREQ_BOUND = 59.000, UPPER_FREQ_BOUND = 61.000;

// Homing Configuration
const bool HOME_DIR_Y = LOW, HOME_DIR_X = LOW;
const int HOMING_STEP_DELAY_US = 500;

// --- Global Variables ---
volatile long currentPositionY = 0, currentPositionX = 0, currentRow = 0;
volatile bool limitY_hit = false, limitX_hit = false;
volatile unsigned long lastEdgeTime = 0;
volatile unsigned long currentEdgeTime = 0;
float measuredFrequency = 0.0;
int FreqqyPacketFlag = 0;
bool freqBreakerState = true;
long medianFreq;
unsigned long lastPrintTime = 0, lastReconnectAttempt = 0;

WiFiManager wifiManager;
RunningMedian samples = RunningMedian(60);
WebsocketsClient client;

// --- Interrupt Service Routines ---
void IRAM_ATTR isrY() { limitY_hit = true; }
void IRAM_ATTR isrX() { limitX_hit = true; }
void IRAM_ATTR onEdge() {
    currentEdgeTime = micros();
    unsigned long interval = currentEdgeTime - lastEdgeTime;

    if (interval > 100 && interval < 10000000) {
        lastEdgeTime = currentEdgeTime;
        measuredFrequency = 1000000.0 / interval;
    } else {
        measuredFrequency = 0.0;
    }
    samples.add(measuredFrequency);
}

// --- Helper Motor Functions ---
void moveStepper(int stepPin, int stepDelayUs) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(stepDelayUs);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(stepDelayUs);
}

void moveToStartPosition(char axis, int startPosition, int stepDelayUs) {
    Serial.printf("Moving %c axis to start position...\n", axis);
    moveToPosition(axis, startPosition, stepDelayUs);
    delay(1000);
}

void moveToSwitchPosition(char axis, long position, int stepDelayUs) {
    Serial.printf("Moving %c axis to switch position: %ld...\n", axis, position);
    moveToPosition(axis, position, stepDelayUs);
    delay(1000);
}

void flipSwitchAction(char axis, long flipPosition, int stepDelayUs) {
    Serial.printf("Flipping switch on %c axis at position: %ld...\n", axis, flipPosition);
    moveToPosition(axis, flipPosition, stepDelayUs);
    delay(1000);
}

// --- Motor Functions ---
void homeAxis(const char* axisName, int stepPin, int dirPin, int enPin, uint8_t limitPin, volatile bool &limitHitFlag, volatile long &currentPosition, bool homeDirection, int stepDelayUs) {
    Serial.printf("Homing %s axis...\n", axisName);

    limitHitFlag = false;
    digitalWrite(dirPin, homeDirection);
    digitalWrite(enPin, LOW);
    delay(5);

    while (!limitHitFlag) {
        moveStepper(stepPin, stepDelayUs);
        yield();
    }

    delay(10);
    digitalWrite(enPin, HIGH);

    if (limitHitFlag) {
        Serial.printf("%s axis homed successfully.\n", axisName);
        currentPosition = 0;
    } else {
        Serial.printf("Error: Homing failed for %s axis.\n", axisName);
        while (1) {
            Serial.println("HOMING FAILED - HALTED");
            delay(1000);
        }
    }

    limitHitFlag = false;
    delay(100);
}

void flipSwitch(int switchNumber, char onOff) {
    long movenumb = (7500 + 2500 * switchNumber);
    int startRow = (switchNumber % 2 == 0) ? 1 : 0;
    int flipRow = (switchNumber % 2 == 0) ? 2 : 1;
    Serial.printf("Turning breaker %d %c\n", switchNumber, onOff);
    if (onOff == 'N') { // Turn on the switch
        if (currentRow != startRow) {
            moveToStartPosition('Y', 5000, 150);
            currentRow = startRow;
        }
        moveToSwitchPosition('X', 1000, 250);
        moveToSwitchPosition('Y', movenumb, 150);
        flipSwitchAction('X', 4000, 500);
        moveToSwitchPosition('X', 1000, 250);
    } else { // Turn off the switch
        if (currentRow != flipRow) {
            moveToStartPosition('Y', 5000, 150);
            currentRow = flipRow;
        }
        moveToSwitchPosition('X', 9300, 250);
        moveToSwitchPosition('Y', movenumb, 150);
        flipSwitchAction('X', 5300, 500);
        moveToSwitchPosition('X', 9300, 250);
    }
}
  
void moveToPosition(char axis, long targetPosition, int stepDelayUs) {
    int stepPin, dirPin, enPin;
    volatile long *currentPositionPtr;
    uint8_t limitPin;
    volatile bool *limitHitFlagPtr;
    bool homeDirection, positiveDirectionIsHigh;

    // Configure based on axis
    if (axis == 'X' || axis == 'x') {
        stepPin = stepPinX; dirPin = dirPinX; enPin = enPinX;
        limitPin = limitPinX; limitHitFlagPtr = &limitX_hit; currentPositionPtr = &currentPositionX;
        homeDirection = HOME_DIR_X;
    } else if (axis == 'Y' || axis == 'y') {
        stepPin = stepPinY; dirPin = dirPinY; enPin = enPinY;
        limitPin = limitPinY; limitHitFlagPtr = &limitY_hit; currentPositionPtr = &currentPositionY;
        homeDirection = HOME_DIR_Y;
    } else {
        Serial.println("Error: Invalid axis specified in moveToPosition.");
        return;
    }

    positiveDirectionIsHigh = (homeDirection == LOW);
    long currentPosValue = *currentPositionPtr;
    long stepsNeeded = targetPosition - currentPosValue;

    if (stepsNeeded == 0) {
        Serial.printf("Axis %c already at target position: %ld\n", axis, targetPosition);
        return;
    }

    bool movingPositive = stepsNeeded > 0;
    digitalWrite(dirPin, movingPositive ? positiveDirectionIsHigh : !positiveDirectionIsHigh);
    long numStepsToTake = abs(stepsNeeded);

    Serial.printf("Axis %c moving from %ld to %ld (Steps: %ld)\n", axis, currentPosValue, targetPosition, stepsNeeded);

    digitalWrite(enPin, LOW);
    delay(1);

    for (long i = 0; i < numStepsToTake; i++) {
        if (!movingPositive && *limitHitFlagPtr) {
            Serial.println("\nEMERGENCY STOP - Limit hit while moving towards home!");
            *limitHitFlagPtr = true;
            break;
        }

        moveStepper(stepPin, stepDelayUs);

        if (movingPositive) (*currentPositionPtr)++;
        else (*currentPositionPtr)--;

        yield();
    }

    digitalWrite(enPin, HIGH);
    *limitHitFlagPtr = false;
    Serial.printf("Final position: %ld\n", *currentPositionPtr);
}

// --- WebSocket Functions ---
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

void sendWebSocketMessage(const char* command, int breakerNum = -1, bool breakerState = -1) {
    StaticJsonDocument<128> doc;
    doc["command"] = command;
    doc["mac_addr"] = WiFi.macAddress();
    if (breakerNum != -1) doc["breakerNum"] = breakerNum;
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
            doc["frequency"] = medianFreq;

            char messageBuffer[128];
            serializeJson(doc, messageBuffer);
            client.send(messageBuffer);
        }

        FreqqyPacketFlag++;
        Serial.printf("Sent Frequency: %.3f Hz\n", medianFreq);
    }
}

// Handle LED Blinking Based on Frequency
void handleFrequencyRoutine() {
    if (samples.isFull()){
      medianFreq = samples.getMedian();
    }

    //for (int i = 0; i<samples.getSize();i++){
    //  Serial.printf("Element %d: %f\n", i, samples.getElement(i));
    //}

    if (medianFreq < LOWER_FREQ_BOUND || medianFreq > UPPER_FREQ_BOUND || medianFreq == 0.0f) {
      if (freqBreakerState) {
            flipSwitch(1, 'F');
            sendWebSocketMessage("toggleBreaker", 1, false);
            freqBreakerState = !freqBreakerState;
            Serial.println("Frequency out of range, turning off breaker.");
      } else if (!freqBreakerState) {
            flipSwitch(1, 'N'); // Turn on the breaker
            sendWebSocketMessage("toggleBreaker", 1, true);
            freqBreakerState = !freqBreakerState;
            Serial.println("Frequency out of range, turning on breaker.");
      }
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
    int breakerNum = doc["breakerNum"] | -1;
    bool breakerState = doc["breakerState"] | false;
    char switchParam;

    if (strcmp(command, "pingDevice") == 0) {
        sendWebSocketMessage("ACK");
    } 
    else if (strcmp(command, "toggleBreaker") == 0 && breakerNum != -1) {
        if (breakerState == false) switchParam = 'F';
        else if (breakerState == true) switchParam = 'N';
        flipSwitch(breakerNum, switchParam);
        sendWebSocketMessage(command, breakerNum, breakerState);
    } 
    else {
        Serial.println("Unknown Command Received.");
    }
}

// Setup Function
void setup() {
    Serial.begin(115200);

    // Uncomment this to reset saved Wi-Fi credentials
    // wifiManager.resetSettings(); 

    if (!wifiManager.autoConnect("ESP32-Config", "password")) {
        Serial.println("Wi-Fi connection failed. Restarting...");
        ESP.restart();
    }

    // Configure Y and X axis, Limit switches, and Frequency sensing pin
    pinMode(stepPinY, OUTPUT); pinMode(dirPinY, OUTPUT); pinMode(enPinY, OUTPUT);
    pinMode(stepPinX, OUTPUT); pinMode(dirPinX, OUTPUT); pinMode(enPinX, OUTPUT);
    pinMode(limitPinY, INPUT_PULLDOWN); pinMode(limitPinX, INPUT_PULLDOWN);
    pinMode(SIGNAL_IN_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(limitPinY), isrY, RISING);
    attachInterrupt(digitalPinToInterrupt(limitPinX), isrX, RISING);
    attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN), onEdge, RISING);

    Serial.println("Connected to Wi-Fi!");
    client.onMessage(onMessageCallback);
    connectWebSocket();

    Serial.println("Starting homing procedure...");
    Serial.print("Y State: ");
    Serial.println(digitalRead(limitPinY));
    Serial.print("Y FLAG: ");
    Serial.println(limitY_hit);
    Serial.print("X State: ");
    Serial.println(digitalRead(limitPinX));
    Serial.print("X FLAG: ");
    Serial.println(limitX_hit);

    homeAxis("Y", stepPinY, dirPinY, enPinY, limitPinY, limitY_hit, currentPositionY, HOME_DIR_Y, HOMING_STEP_DELAY_US);

    Serial.println("Post Hone Y");
    Serial.print("Y FLAG: ");
    Serial.println(limitY_hit);
    Serial.print("X FLAG: ");
    Serial.println(limitX_hit);
    
    homeAxis("X", stepPinX, dirPinX, enPinX, limitPinX, limitX_hit, currentPositionX, HOME_DIR_X, HOMING_STEP_DELAY_US);
    currentRow = 0; // set correct row

    Serial.println("\nSetup complete. Both axes homed.");
    Serial.print("Initial Position X: "); Serial.println(currentPositionX);
    Serial.print("Initial Position Y: "); Serial.println(currentPositionY);
    
}

// Main Loop
void loop() {
    handleWebSocket();
    handleFrequencyRoutine();
    sendFrequencyUpdate();
}