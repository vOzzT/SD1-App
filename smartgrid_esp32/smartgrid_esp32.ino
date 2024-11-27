#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
const int ledPin = 2;  // GPIO for built-in LED

const int localPort = 12345; // Local port to listen on
char incomingPacket[255];      // Buffer for incoming packets

//Your IP address or domain name with URL path
const char* serverName = "http://smartgrid-app.xyz";

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Ensure the LED is off 

  // Initialize WiFiManager
  WiFiManager wifiManager;  

  // Uncomment this to reset saved Wi-Fi credentials
  // wifiManager.resetSettings(); 

  // Automatically connect to last used Wi-Fi or open a captive portal if not connected
  if(!wifiManager.autoConnect("ESP32-Config", "password")) {
    Serial.println("Failed to connect to Wi-Fi and hit timeout");
    ESP.restart();
  } 

  // If you get here, you're connected to the Wi-Fi
  Serial.println("Connected to Wi-Fi!");  

  // Define the HTTP POST handler for the /led endpoint
    server.on("/led", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Blink Led 10 Times
        for (int i=0; i<10; i++){
            delay(100);
            digitalWrite(ledPin,HIGH);
            delay(100);
            digitalWrite(ledPin,LOW);
        }
        request->send(200, "application/json", "{\"status\": \"LED on\"}");
    });

    server.begin(); 

}

void loop() {
    // The main loop can remain empty as the server runs in the background
}

