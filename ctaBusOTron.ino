/*
 * CTA Bus-O-Tron - Optimized Version with Secure Configuration
 * ESP8266-based transit arrival indicator for Chicago CTA buses and trains
 * 
 * This device connects to WiFi and subscribes to MQTT topics containing
 * real-time arrival predictions for various CTA routes. It displays the
 * proximity of arriving vehicles using colored LEDs and directional arrows
 * that blink at different speeds based on arrival time.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"  // Include network credentials from separate file

// GPIO Pin Definitions for ESP8266
#define PIN_ARROW_RIGHT 2    // Right directional arrow LED
#define PIN_ARROW_LEFT 16    // Left directional arrow LED  
#define PIN_AMBER_RIGHT 0    // Right amber route indicator LED
#define PIN_AMBER_LEFT 5     // Left amber route indicator LED
#define PIN_TALL_LIGHT 15    // Status/alert indicator (tall light)
#define PIN_RED 4            // Red line indicator LED (used for Brown Line)

// Network Configuration Structure
// Contains all WiFi and MQTT connection parameters loaded from secrets.h
struct NetworkConfig {
  const char* ssid = WIFI_SSID;                    // WiFi network name from secrets.h
  const char* password = WIFI_PASSWORD;            // WiFi password from secrets.h
  const char* mqttServer = MQTT_SERVER;            // MQTT broker address from secrets.h
  const int mqttPort = MQTT_PORT;                  // MQTT broker port from secrets.h
  const char* mqttUser = MQTT_USER;                // MQTT username from secrets.h
  const char* mqttPass = MQTT_PASSWORD;            // MQTT password from secrets.h
  const char* clientId = MQTT_CLIENT_ID;           // MQTT client identifier from secrets.h
};

// Enumerations for type safety and code clarity
enum RouteType { BUS_ROUTE, RAIL_ROUTE };                    // Type of transit route
enum LightColor { AMBER_LEFT, AMBER_RIGHT, RED_LIGHT };      // Available LED colors
enum ArrowDirection { ARROW_LEFT, ARROW_RIGHT, NO_ARROW };   // Arrow display directions

// Route Structure
// Defines a single transit route with all its display properties
struct Route {
  const char* name;         // Human-readable route name for debugging
  const char* topic;        // MQTT topic to subscribe to for this route
  RouteType type;           // Bus or rail route type
  LightColor color;         // Which colored LED to illuminate
  ArrowDirection arrow;     // Which arrow direction to show
  int displaySlot;          // Time slot (0-5) when this route is displayed
  int eta;                  // Current estimated arrival time in seconds
  
  // Constructor to initialize all route properties
  Route(const char* n, const char* t, RouteType rt, LightColor c, ArrowDirection a, int slot) 
    : name(n), topic(t), type(rt), color(c), arrow(a), displaySlot(slot), eta(0) {}
};

/*
 * LED Controller Class
 * 
 * Manages all LED operations including initialization, state control,
 * and dynamic blinking patterns based on arrival times.
 * 
 * Blinking speed indicates urgency:
 * - Super fast (180ms): Vehicle arriving very soon (< 90-180 seconds)
 * - Fast (360ms): Vehicle approaching (< 300-360 seconds)  
 * - Medium (600ms): Moderate wait time (< 600 seconds)
 * - Slow (900ms): Longer wait time (< 900 seconds)
 * - Solid on: Very distant arrival (< 5000 seconds)
 * - Off: No data or extremely distant (> 5000 seconds)
 */
class LEDController {
private:
  // Blink interval constants in milliseconds
  static const int BLINK_SUPER_FAST = 180;  // Imminent arrival
  static const int BLINK_FAST = 360;        // Approaching
  static const int BLINK_MEDIUM = 600;      // Moderate wait
  static const int BLINK_SLOW = 900;        // Longer wait
  
public:
  // Initialize all LED pins and set them to a known state
  void init() {
    pinMode(PIN_ARROW_RIGHT, OUTPUT);
    pinMode(PIN_ARROW_LEFT, OUTPUT);
    pinMode(PIN_AMBER_RIGHT, OUTPUT);
    pinMode(PIN_AMBER_LEFT, OUTPUT);
    pinMode(PIN_TALL_LIGHT, OUTPUT);
    pinMode(PIN_RED, OUTPUT);
    
    turnOffAll();                          // Start with all route LEDs off
    digitalWrite(PIN_TALL_LIGHT, HIGH);    // Status light on during startup
  }
  
  // Turn off all route indicator LEDs (but not status light)
  void turnOffAll() {
    digitalWrite(PIN_ARROW_RIGHT, LOW);
    digitalWrite(PIN_ARROW_LEFT, LOW);
    digitalWrite(PIN_AMBER_RIGHT, LOW);
    digitalWrite(PIN_AMBER_LEFT, LOW);
    digitalWrite(PIN_RED, LOW);
  }
  
  // Control the tall status/alert light
  void setStatusLight(bool on) {
    digitalWrite(PIN_TALL_LIGHT, on ? HIGH : LOW);
  }
  
  // Set the color indicator LED for the current route
  void setColorLight(LightColor color, bool on) {
    switch(color) {
      case AMBER_LEFT:
        digitalWrite(PIN_AMBER_LEFT, on ? HIGH : LOW);
        break;
      case AMBER_RIGHT:
        digitalWrite(PIN_AMBER_RIGHT, on ? HIGH : LOW);
        break;
      case RED_LIGHT:
        digitalWrite(PIN_RED, on ? HIGH : LOW);
        break;
    }
  }
  
  // Update arrow blinking based on ETA and current time
  // This creates the dynamic blinking pattern that indicates arrival urgency
  void updateArrowBlink(ArrowDirection direction, int eta, unsigned long currentTime) {
    if (eta <= 0) {
      setArrow(direction, false);  // No data - turn off arrow
      return;
    }
    
    int blinkInterval = getBlinkInterval(eta);
    if (blinkInterval == 0) {
      // Very distant - turn off arrow
      setArrow(direction, false);
      return;
    }
    
    if (blinkInterval == -1) {
      // Distant but show solid arrow
      setArrow(direction, true);
      return;
    }
    
    // Create blinking pattern: on for first half of interval, off for second half
    bool shouldBeOn = (currentTime % blinkInterval) < (blinkInterval / 2);
    setArrow(direction, shouldBeOn);
  }
  
private:
  // Set arrow LED state and ensure only one arrow is on at a time
  void setArrow(ArrowDirection direction, bool on) {
    switch(direction) {
      case ARROW_LEFT:
        digitalWrite(PIN_ARROW_LEFT, on ? HIGH : LOW);
        digitalWrite(PIN_ARROW_RIGHT, LOW);  // Ensure other arrow is off
        break;
      case ARROW_RIGHT:
        digitalWrite(PIN_ARROW_RIGHT, on ? HIGH : LOW);
        digitalWrite(PIN_ARROW_LEFT, LOW);   // Ensure other arrow is off
        break;
      case NO_ARROW:
        digitalWrite(PIN_ARROW_LEFT, LOW);
        digitalWrite(PIN_ARROW_RIGHT, LOW);
        break;
    }
  }
  
  // Determine blink interval based on ETA
  // Returns: milliseconds for blink interval, -1 for solid on, 0 for off
  int getBlinkInterval(int eta) {
    if (eta < 90) return BLINK_SUPER_FAST;   // < 1.5 minutes
    if (eta < 300) return BLINK_FAST;        // < 5 minutes
    if (eta < 600) return BLINK_MEDIUM;      // < 10 minutes
    if (eta < 900) return BLINK_SLOW;        // < 15 minutes
    if (eta < 5000) return -1;               // < ~83 minutes - show solid
    return 0;                                // Very distant - turn off
  }
};

/*
 * Network Manager Class
 * 
 * Handles all network communications including WiFi connection,
 * MQTT broker connection with automatic reconnection, and
 * message routing to the application callback function.
 */
class NetworkManager {
private:
  WiFiClient wifiClient;       // WiFi client for network connection
  PubSubClient mqttClient;     // MQTT client for pub/sub messaging
  NetworkConfig config;        // Network configuration parameters
  std::function<void(char*, byte*, unsigned int)> messageCallback;  // App callback for messages
  
public:
  NetworkManager() : mqttClient(wifiClient) {}
  
  // Initialize network connections and set up message callback
  void init(std::function<void(char*, byte*, unsigned int)> callback) {
    messageCallback = callback;
    connectWiFi();
    setupMQTT();
    connectMQTT();  // Initial connection attempt

  }
  
  // Main network loop - handles reconnections and processes MQTT messages
  // Should be called frequently from main loop
  void loop() {
    if (!mqttClient.connected()) {
      reconnectMQTT();  // Attempt to reconnect if connection lost
    }
    mqttClient.loop();  // Process incoming MQTT messages
  }
  
  // Subscribe to an MQTT topic for receiving messages
  void subscribe(const char* topic) {
      bool result = mqttClient.subscribe(topic);
      Serial.print("Subscribing to: ");
      Serial.print(topic);
      Serial.print(" - ");
      Serial.println(result ? "SUCCESS" : "FAILED");
  }
  
private:
  // Connect to WiFi network with status feedback
  void connectWiFi() {
    WiFi.begin(config.ssid, config.password);
    Serial.println("Connecting to WiFi");
    Serial.println(config.ssid);
    
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(100);
    }
    
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Configure MQTT client with server details and callback
  void setupMQTT() {
    mqttClient.setServer(config.mqttServer, config.mqttPort);
    
    // Set up lambda callback to forward messages to application
    mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
      if (messageCallback) {
        messageCallback(topic, payload, length);
      }
    });
  }
  
    // Connect to MQTT broker
  void connectMQTT() {
    Serial.print("Attempting MQTT connection...");
    
  // Generate unique client ID to avoid conflicts
    String clientId = String(config.clientId) + "-" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), config.mqttUser, config.mqttPass)) {
      Serial.println("MQTT connected successfully!");
      Serial.print("Client ID: ");
      Serial.println(clientId);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
  
  // Attempt to reconnect to MQTT broker
  void reconnectMQTT() {
    if (mqttClient.connect(config.clientId, config.mqttUser, config.mqttPass)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.println(mqttClient.state());
      delay(2000);  // Wait before retry
    }
  }
};

/*
 * Display Manager Class
 * 
 * Manages the cycling display system that rotates through different
 * transit routes every 10 seconds in a 60-second cycle.
 * 
 * Display cycle (60 seconds total):
 * - Seconds 0-9:   Slot 0 - Downtown Express
 * - Seconds 10-19: Slot 1 - Route 77 westbound  
 * - Seconds 20-29: Slot 2 - Brown Line southbound
 * - Seconds 30-39: Slot 3 - Brown Line northbound
 * - Seconds 40-49: Slot 4 - Route 151 southbound
 * - Seconds 50-59: Slot 5 - Route 151 northbound
 */
class DisplayManager {
private:
  static const int DISPLAY_CYCLE_LENGTH = 60000;  // Total cycle length: 60 seconds
  static const int SLOT_DURATION = 10000;         // Each route displayed for 10 seconds
  static const int STATUS_PRINT_INTERVAL = 5000;  // Print debug status every 5 seconds
  
  LEDController& ledController;  // Reference to LED controller
  Route* routes;                 // Array of route definitions
  int routeCount;               // Number of routes in array
  unsigned long lastStatusPrint; // Timestamp of last status print
  
public:
  DisplayManager(LEDController& leds, Route* routeArray, int count) 
    : ledController(leds), routes(routeArray), routeCount(count), lastStatusPrint(0) {}
  
  // Main update function - determines current display slot and updates LEDs
  void update(unsigned long currentTime, bool enabled) {
    if (!enabled) {
      ledController.turnOffAll();  // System disabled - turn off all route LEDs
      return;
    }
    
    // Print status periodically for debugging
    if (currentTime - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
      printStatus();
      lastStatusPrint = currentTime;
    }
    
    // Calculate current display slot (0-5) based on time within 60-second cycle
    int currentSlot = (currentTime % DISPLAY_CYCLE_LENGTH) / SLOT_DURATION;
    
    // Find the route assigned to current display slot
    Route* currentRoute = nullptr;
    for (int i = 0; i < routeCount; i++) {
      if (routes[i].displaySlot == currentSlot) {
        currentRoute = &routes[i];
        break;
      }
    }
    
    // Clear all LEDs before displaying current route
    ledController.turnOffAll();
    
    // Display current route if found and has valid ETA data
    if (currentRoute && currentRoute->eta > 0) {
      ledController.setColorLight(currentRoute->color, true);  // Set route color
      ledController.updateArrowBlink(currentRoute->arrow, currentRoute->eta, currentTime);  // Animate arrow
    }
  }
  
private:
  // Print current ETA status for all routes (debugging)
  void printStatus() {
    Serial.println("\n=== CURRENT ETAs ===");
    for (int i = 0; i < routeCount; i++) {
      Serial.print(routes[i].name);
      Serial.print(": ");
      Serial.print(routes[i].eta);
      Serial.println(" seconds");
    }
    Serial.println("==================");
  }
};

/*
 * Main Bus-O-Tron Class
 * 
 * Orchestrates all system components and handles MQTT message processing.
 * This is the main application class that ties together LED control,
 * network management, and display timing.
 */
class BusOTron {
private:
  // Class constants
  static const int ROUTE_COUNT = 6;  // Number of routes in the system
  
  LEDController ledController;     // Manages all LED operations
  NetworkManager networkManager;   // Handles WiFi and MQTT connections
  DisplayManager displayManager;   // Controls display cycling and timing
  
  bool systemEnabled;             // Master enable/disable flag
  bool alertActive;              // Alert status for status light
  
  // Route definitions array
  // Each route specifies: name, MQTT topic, type, color LED, arrow direction, display slot
  Route routes[ROUTE_COUNT] = {
    Route("Downtown Express", "CTApredictions/BUS/dtwnEXP", BUS_ROUTE, AMBER_LEFT, ARROW_LEFT, 0),
    Route("Route 77 West", "CTApredictions/BUS/1151/77", BUS_ROUTE, AMBER_LEFT, ARROW_RIGHT, 1),
    Route("Brown Line South", "CTApredictions/RAIL/30232", RAIL_ROUTE, RED_LIGHT, ARROW_LEFT, 2),
    Route("Brown Line North", "CTApredictions/RAIL/30231", RAIL_ROUTE, RED_LIGHT, ARROW_RIGHT, 3),
    Route("Route 151 South", "CTApredictions/BUS/1074/151", BUS_ROUTE, AMBER_RIGHT, ARROW_LEFT, 4),
    Route("Route 151 North", "CTApredictions/BUS/1151/151", BUS_ROUTE, AMBER_RIGHT, ARROW_RIGHT, 5)
  };
  
public:
  BusOTron() : displayManager(ledController, routes, ROUTE_COUNT), systemEnabled(true), alertActive(false) {}
  
  // Initialize all subsystems and establish network connections
  void init() {
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize
    Serial.println("\n=== CTA Bus-O-Tron Starting ===");
    
    // Initialize LED controller and set initial states
    ledController.init();
    
    // Initialize network manager with message callback
    networkManager.init([this](char* topic, byte* payload, unsigned int length) {
      handleMQTTMessage(topic, payload, length);
    });
    
    delay(2000);
    
    // Subscribe to all necessary MQTT topics
    subscribeToTopics();
    
    Serial.println("=== Bus-O-Tron Ready ===");
  }
  
  // Main execution loop - processes network messages and updates display
  void loop() {
    // Process network communications (WiFi/MQTT)
    networkManager.loop();
    
    // Update display based on current time and system state
    unsigned long currentTime = millis();
    displayManager.update(currentTime, systemEnabled);
    
    // Control status light based on alert state
    ledController.setStatusLight(alertActive);
    
    delay(10);  // Small delay for system stability
  }
  
private:
  // Subscribe to all MQTT topics for system control and route predictions
  void subscribeToTopics() {
    // System control topics
    networkManager.subscribe("devices/busotron/enable");      // Master enable/disable
    networkManager.subscribe("CTApredictions/alert/active");  // Alert system control
    
    // Route prediction topics - subscribe to each route's MQTT topic
    for (int i = 0; i < ROUTE_COUNT; i++) {
      networkManager.subscribe(routes[i].topic);
    }
  }
  
  // Handle incoming MQTT messages and update system state
  void handleMQTTMessage(char* topic, byte* payload, unsigned int length) {
    // Convert binary payload to null-terminated string
    char message[16] = {0};  // Buffer for incoming message
    if (length < sizeof(message)) {
      memcpy(message, payload, length);
      message[length] = '\0';
    } else {
      // Handle oversized messages safely
      Serial.println("Message too long, truncating");
      memcpy(message, payload, sizeof(message) - 1);
      message[sizeof(message) - 1] = '\0';
    }
    
    // Log received message for debugging
    Serial.print("Topic: ");
    Serial.print(topic);
    Serial.print(" | Message: ");
    Serial.println(message);
    
    // Handle system control messages
    if (strcmp(topic, "devices/busotron/enable") == 0) {
      // Master system enable/disable
      systemEnabled = (strcmp(message, "ON") == 0);
      Serial.print("System ");
      Serial.println(systemEnabled ? "ENABLED" : "DISABLED");
      return;
    }
    
    if (strcmp(topic, "CTApredictions/alert/active") == 0) {
      // Alert system control (affects status light)
      alertActive = (strcmp(message, "ON") == 0);
      Serial.print("Alert ");
      Serial.println(alertActive ? "ACTIVE" : "INACTIVE");
      return;
    }
    
    // Handle route prediction messages (ETA updates)
    int eta = String(message).toInt();  // Convert message to integer seconds
    for (int i = 0; i < ROUTE_COUNT; i++) {
      if (strcmp(topic, routes[i].topic) == 0) {
        routes[i].eta = eta;  // Update ETA for matching route
        Serial.print("Updated ");
        Serial.print(routes[i].name);
        Serial.print(" ETA: ");
        Serial.println(eta);
        return;
      }
    }
    
    // Log unknown topics for debugging
    Serial.println("Unknown topic");
  }
};

// Global Bus-O-Tron instance
BusOTron busOTron;

// Arduino setup function - called once at startup
void setup() {
  busOTron.init();
}

// Arduino main loop function - called continuously
void loop() {
  busOTron.loop();
}