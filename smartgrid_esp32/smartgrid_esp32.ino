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

// Define Stepper Pins
const int stepPinY = 21;
const int dirPinY = 2;
const int enPinY = 23;

const int stepPinX = 19;
const int dirPinX = 18;
const int enPinX = 5;

// Define Limit Switch Pins
const uint8_t limitPinY = 15;
const uint8_t limitPinX = 22;

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
bool freqBreakerState = true;
unsigned long lastPrintTime = 0;

// ** ADJUST THESE AS NEEDED **
const bool HOME_DIR_Y = LOW; // e.g., LOW moves towards Y limit
const bool HOME_DIR_X = LOW; // e.g., LOW moves towards X limit

// Set the speed for homing (delay between steps in microseconds)
const int HOMING_STEP_DELAY_US = 500; // Slow speed for accuracy

// --- Global State Variables ---
volatile long currentPositionY = 0; // Position relative to home (0)
volatile long currentPositionX = 0; // Position relative to home (0)
volatile long currentRow = 0;// 0-left, 1-miidle, 3- right

volatile bool limitY_hit = false;
volatile bool limitX_hit = false;

WiFiManager wifiManager;

// Interrupt Service Routines (ISRs) for Motor Control 
void IRAM_ATTR isrY() {
    limitY_hit = true;
}

void IRAM_ATTR isrX() {
    limitX_hit = true;
}

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

// --- Homing Function (Unchanged) ---
void homeAxis(const char* axisName, int stepPin, int dirPin, int enPin, uint8_t limitPin, volatile bool &limitHitFlag, volatile long currentPosition, bool homeDirection, int stepDelayUs) {
    Serial.print("Homing ");
    Serial.print(axisName);
    Serial.println(" axis...");

    limitHitFlag = false;
    digitalWrite(dirPin, homeDirection);
    digitalWrite(enPin, LOW);
    delay(5);

    while (digitalRead(limitPin) == HIGH && !limitHitFlag) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(stepDelayUs);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(stepDelayUs);
        yield();
    }
    delay(10);
    digitalWrite(enPin, HIGH);

    if (limitHitFlag || digitalRead(limitPin) == LOW) { // Check flag OR direct read
        Serial.print(axisName);
        Serial.println(" axis homed successfully.");
        currentPosition = 0; // CRITICAL: Set position to 0 after homing
    } else {
        Serial.print("Error: Homing failed for ");
        Serial.print(axisName);
        Serial.println(" axis.");
        while(1) { Serial.println("HOMING FAILED - HALTED"); delay(1000); }
    }
    limitHitFlag = false; // Clear flag for normal operation
    delay(100);
}

void flipSwitch(int switchnumb, char onoff ){
    if (switchnumb % 2 != 0) { //if its odd, then its the left collum of switches
      if (onoff == 'N' ) { //this is the case for turning one of those switches on
        if (currentRow != 0) { //if not on the correct row, move there
          // move y into start position
          moveToPosition('Y', 5000, 150); 
          delay(1000);
        }
  
        //move x into position to flip switch on left to right
        moveToPosition('X', 1000, 250);
        delay(1000);
  
        currentRow = 0;
  
        long movenumb = (7500 + 2500*switchnumb);
  
        // move y into switch position
        moveToPosition('Y', movenumb, 150); 
        delay(1000);
    
        //move X to flip
        moveToPosition('X', 4000, 500); 
        delay(1000);
  
        //move x away
        moveToPosition('X', 1000, 250);
        delay(1000);
      } else {
          if(currentRow != 1){ //if not on the correct row, move there
            // move y into start position
            moveToPosition('Y', 5000, 150); 
            delay(1000);
          }
      
          //move x into position to flip switch on left to right
          moveToPosition('X', 9300, 250);
          delay(1000);
  
          currentRow = 1;
  
          long movenumb = (7500 + 2500*switchnumb);
  
          // move y into switch position
          moveToPosition('Y', movenumb, 150); 
          delay(1000);
  
          //move X to flip
          moveToPosition('X', 5300, 500); 
          delay(1000);
  
          //move x away
          moveToPosition('X', 9300, 250);
          delay(1000);
        }
    } else {// if it is even
        if (onoff == 'F' ) { //this is the case for turning one of those switches on
          if(currentRow != 1) { //if not on the correct row, move there
   
            // move y into start position
            moveToPosition('Y', 5000, 150); 
            delay(1000);
          }
   
          //move x into position to flip switch on left to right
          moveToPosition('X', 9300, 250);
          delay(1000);
  
          currentRow = 1;
  
          long movenumb = (5000 + 2500*switchnumb);
        
           // move y into switch position
           moveToPosition('Y', movenumb, 150); 
           delay(1000);
        
           //move X to flip
           moveToPosition('X', 13300, 500); 
           delay(1000);
        
           //move x away
           moveToPosition('X', 9300, 250);
           delay(1000);
        } else {
            if(currentRow != 2) { //if not on the correct row, move there
              // move y into start position
              moveToPosition('Y', 5000, 150); 
              delay(1000);
            }
   
            //move x into position to flip switch on left to right
            moveToPosition('X', 17600, 250);
            delay(1000);
  
            currentRow = 2;
  
            long movenumb = (5000 + 2500*switchnumb);
  
            // move y into switch position
            moveToPosition('Y', movenumb, 150); 
            delay(1000);
        
            //move X to flip
            moveToPosition('X', 14600, 500); 
            delay(1000);
  
            //move x away
            moveToPosition('X', 17600, 250);
            delay(1000);
        }
    }
  }
  
  // --- Helper function for moving to a specific absolute position ---
  void moveToPosition(char axis, long targetPosition, int stepDelayUs) {
    int stepPin, dirPin, enPin;
    volatile long *currentPositionPtr; // Pointer to the axis's current position
    uint8_t limitPin;
    volatile bool *limitHitFlagPtr;
    bool homeDirection;         // Direction used for homing this axis
    bool positiveDirectionIsHigh; // Is DIR pin HIGH for positive movement?
    // --- Configure based on axis ---
    if (axis == 'X' || axis == 'x') {
        stepPin = stepPinX;
        dirPin = dirPinX;
        enPin = enPinX;
        limitPin = limitPinX;
        limitHitFlagPtr = &limitX_hit;
        currentPositionPtr = &currentPositionX;
        homeDirection = HOME_DIR_X;
    } else if (axis == 'Y' || axis == 'y') {
        stepPin = stepPinY;
        dirPin = dirPinY;
        enPin = enPinY;
        limitPin = limitPinY;
        limitHitFlagPtr = &limitY_hit;
        currentPositionPtr = &currentPositionY;
        homeDirection = HOME_DIR_Y;
    } else {
        Serial.println("Error: Invalid axis specified in moveToPosition.");
        return; // Invalid axis
    }
  
      // Determine which DIR state corresponds to positive movement (away from home)
      // If homing direction is LOW, then positive direction is HIGH.
      // If homing direction is HIGH, then positive direction is LOW.
      positiveDirectionIsHigh = (homeDirection == LOW);
  
      // --- Calculate movement ---
      long currentPosValue = *currentPositionPtr; // Get the current position value
      long stepsNeeded = targetPosition - currentPosValue;
  
      // --- Handle no movement required ---
      if (stepsNeeded == 0) {
          Serial.print("Axis "); Serial.print(axis);
          Serial.print(" already at target position: "); Serial.println(targetPosition);
          return;
      }
  
      // --- Determine direction and number of steps ---
      bool movingPositive = stepsNeeded > 0; // Moving away from 0 towards positive numbers
      digitalWrite(dirPin, movingPositive ? positiveDirectionIsHigh : !positiveDirectionIsHigh);
  
      long numStepsToTake = abs(stepsNeeded);
  
      Serial.print("Axis "); Serial.print(axis);
      Serial.print(" moving from "); Serial.print(currentPosValue);
      Serial.print(" to "); Serial.print(targetPosition);
      Serial.print(" (Steps: "); Serial.print(stepsNeeded); Serial.println(")");
  
  
      // --- Execute movement ---
      digitalWrite(enPin, LOW); // Enable motor
      delay(1); // Allow driver to enable
  
      bool limit_encountered = false;
      for (long i = 0; i < numStepsToTake; i++) {
          // Check limit switch ONLY if moving towards home (position 0)
          bool isMovingTowardsHome = !movingPositive; // Moving towards negative numbers (or 0)
  
          // Check flag OR direct read. Only trigger stop if moving towards home.
          if (isMovingTowardsHome && (*limitHitFlagPtr || digitalRead(limitPin) == LOW)) {
             Serial.println("\n EMERGENCY STOP - Limit hit while moving towards home!");
             *limitHitFlagPtr = true; // Ensure flag is set
             limit_encountered = true;
  
             // Problem: If limit is hit, the position counter is now potentially wrong.
             // Best practice is usually to require re-homing after a limit hit during a move.
             // For now, we just stop and report the position where we stopped.
             // A more robust system might set an error state.
             break; // Stop motion
          } else if (!isMovingTowardsHome && *limitHitFlagPtr) {
              // If moving away from home and flag is somehow set, clear it.
               *limitHitFlagPtr = false;
          }
  
  
          // Pulse the step pin
          digitalWrite(stepPin, HIGH);
          delayMicroseconds(stepDelayUs);
          digitalWrite(stepPin, LOW);
          delayMicroseconds(stepDelayUs);
  
          // Update the actual position counter based on the direction moved
          // Do this AFTER the step is taken, BEFORE checking the limit for the *next* step
          if (movingPositive) {
              (*currentPositionPtr)++;
          } else {
              (*currentPositionPtr)--;
          }
  
          yield(); // Prevent watchdog resets on long moves
      }
  
      // --- Finish Move ---
      digitalWrite(enPin, HIGH); // Disable motor after move
  
      if (limit_encountered) {
           Serial.print(" Stopped early due to limit switch.");
           // Position was updated one last time before the break happened
      }
      Serial.print(" Final position: "); Serial.println(*currentPositionPtr);
  
      // Clear the limit flag for the next operation, unless it was a real stop
      // If stopped by limit, maybe leave flag set to indicate an issue?
      // For now, we'll clear it. Consider adding error state handling if needed.
      *limitHitFlagPtr = false;
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
    if (measuredFrequency < 50 || measuredFrequency > 500 || measuredFrequency == 0.0f) {
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
    digitalWrite(LED_PIN, LOW);

    // Configure Pins
    pinMode(stepPinY, OUTPUT);
    pinMode(dirPinY, OUTPUT); 
    pinMode(enPinY, OUTPUT);
    pinMode(limitPinY, INPUT_PULLUP); 
    digitalWrite(enPinY, HIGH);


    pinMode(stepPinX, OUTPUT); 
    pinMode(dirPinX, OUTPUT); 
    pinMode(enPinX, OUTPUT);
    pinMode(limitPinX, INPUT_PULLUP); 
    digitalWrite(enPinX, HIGH);

    Serial.println("Attaching interrupts...");
    attachInterrupt(digitalPinToInterrupt(limitPinY), isrY, FALLING);
    attachInterrupt(digitalPinToInterrupt(limitPinX), isrX, FALLING);

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

    Serial.println("Starting homing procedure...");
    homeAxis("Y", stepPinY, dirPinY, enPinY, limitPinY, limitY_hit, currentPositionY, HOME_DIR_Y, HOMING_STEP_DELAY_US);
    homeAxis("X", stepPinX, dirPinX, enPinX, limitPinX, limitX_hit, currentPositionX, HOME_DIR_X, HOMING_STEP_DELAY_US);
    currentRow = 0; // set correct row

    Serial.println("\nSetup complete. Both axes homed.");
    Serial.print("Initial Position X: "); Serial.println(currentPositionX);
    Serial.print("Initial Position Y: "); Serial.println(currentPositionY);
    
}

// Main Loop
void loop() {
    handleWebSocket();
    handleLEDBlinking();
    sendFrequencyUpdate();
}