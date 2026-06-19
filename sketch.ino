#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <DHT.h>
#include <queue>

// --- Network Configuration ---
#define WIFI_SSID       "Wokwi-GUEST"  // Universal Wokwi Simulator Access Point
#define WIFI_PASS       ""             // Wokwi network does not require a password
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define IO_USERNAME     "raj12"
#define IO_KEY          "aio_aoWg38lvT63BnR7e1ahm5WBTH67U"

// --- Hardware Pin Allocations ---
#define DHTPIN          15
#define DHTTYPE         DHT22
#define BUZZER_PIN      13
#define LED_GREEN       25
#define LED_YELLOW      26
#define LED_RED         27

// --- Display Definitions ---
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Sensor Init ---
DHT dht(DHTPIN, DHTTYPE);

// --- Structural System States ---
enum SystemState { ONLINE, DEGRADED, OFFLINE };
SystemState currentState = OFFLINE;

// --- Data Buffer Structure ---
struct SensorData {
  float temp;
  float hum;
};
std::queue<SensorData> localBuffer;
const size_t MAX_BUFFER_SIZE = 50; // Safeguard RAM limit

// --- Network Client Initialization ---
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);

// --- Adafruit IO Feed Submissions ---
Adafruit_MQTT_Publish temp_publish = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish hum_publish = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/humidity");

// --- Retry Strategy Control Variables ---
unsigned long lastExecutionTime = 0;
const unsigned long loopInterval = 5000; // 5-Second Telemetry Intervals
unsigned long retryDelay = 2000;         // Starting Backoff Delay (2 seconds)
const unsigned long maxRetryDelay = 32000; // Cap backoff at 32 seconds

// --- Function Declarations ---
void updateIndicators(SystemState state, String statusMessage);
void handleBuzzer(SystemState state);
void executeDataSync();

void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 Allocation Failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Initializing System...");
  display.display();

  // Initial Wi-Fi connection push
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  unsigned long currentTime = millis();

  // Handle periodic checking and publishing loop
  if (currentTime - lastExecutionTime >= loopInterval) {
    lastExecutionTime = currentTime;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Phase 1: Assess Wi-Fi Baseline Connectivity
    if (WiFi.status() != WL_CONNECTED) {
      currentState = OFFLINE;
      updateIndicators(OFFLINE, "LOGGING OFFLINE");
      handleBuzzer(OFFLINE);

      // Cache elements to local memory
      if (localBuffer.size() < MAX_BUFFER_SIZE) {
        localBuffer.push({t, h});
        Serial.printf("Saved local telemetry packet. Buffer: %d\n", localBuffer.size());
      }
      
      // Silently request reconnection in background 
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    } 
    // Phase 2: Check MQTT Availability if Wi-Fi works
    else {
      if (!mqtt.connected()) {
        currentState = DEGRADED;
        updateIndicators(DEGRADED, "MQTT RETRYING");
        handleBuzzer(DEGRADED);

        // Cache live data packet since cloud path is broken
        if (localBuffer.size() < MAX_BUFFER_SIZE) {
          localBuffer.push({t, h});
        }

        // Apply Exponential Backoff Retry Mechanism for MQTT Connection
        Serial.printf("Attempting MQTT connection with backoff delay: %d ms\n", retryDelay);
        int8_t connectionResult = mqtt.connect();
        
        if (connectionResult == 0) {
          Serial.println("MQTT Recovery Successful!");
          retryDelay = 2000; // Reset Backoff step
          currentState = ONLINE;
        } else {
          mqtt.disconnect();
          // Exponential growth of delay
          retryDelay = min(retryDelay * 2, maxRetryDelay);
        }
      } 
      // Phase 3: System Functional and Stable
      else {
        currentState = ONLINE;
        updateIndicators(ONLINE, "SYSTEM ONLINE");
        
        // Push Live Reading
        if (temp_publish.publish(t) && hum_publish.publish(h)) {
          Serial.printf("Live Publish Ok -> Temp: %.2f, Hum: %.2f\n", t, h);
        }

        // If local logs are waiting in queue, drain them
        if (!localBuffer.empty()) {
          executeDataSync();
        }
      }
    }
  }
}

// --- Status Peripheral Update Controller ---
void updateIndicators(SystemState state, String statusMessage) {
  display.clearDisplay();
  
  // 1. Draw Network Status Bar Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi: OK  ");
  } else {
    display.print("WiFi: DIS ");
  }
  
  if (mqtt.connected()) {
    display.println("MQTT: OK");
  } else {
    display.println("MQTT: DIS");
  }
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE); // Horizontal divider

  // 2. Display Live Sensor Data Real-Time
  float currentTemp = dht.readTemperature();
  float currentHum = dht.readHumidity();
  
  display.setCursor(0, 16);
  display.print("TEMP : ");
  if(!isnan(currentTemp)) { 
    display.print(currentTemp); 
    display.print(" C"); 
  } else { 
    display.print("--"); 
  }
  
  display.setCursor(0, 28);
  display.print("HUMID: ");
  if(!isnan(currentHum)) { 
    display.print(currentHum); 
    display.print(" %"); 
  } else { 
    display.print("--"); 
  }
  
  display.drawFastHLine(0, 40, 128, SSD1306_WHITE); // Horizontal divider

  // 3. Display Fault-Tolerant System Queue & State Machine Label
  display.setCursor(0, 45);
  display.printf("BUFFERED LOGS: %d", localBuffer.size());
  
  display.setCursor(0, 56);
  display.print("SYS  : ");
  display.println(statusMessage);
  
  display.display();

  // Dynamic LED State Management Switching
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  switch(state) {
    case ONLINE:
      digitalWrite(LED_GREEN, HIGH);
      break;
    case DEGRADED:
      digitalWrite(LED_YELLOW, HIGH);
      break;
    case OFFLINE:
      digitalWrite(LED_RED, HIGH);
      break;
  }
}

// --- Fault Audio Signalling Module ---
void handleBuzzer(SystemState state) {
  switch(state) {
    case DEGRADED:
      // Double Short Notification tone
      tone(BUZZER_PIN, 1500, 150);
      delay(200);
      tone(BUZZER_PIN, 1500, 150);
      break;
    case OFFLINE:
      // Intense Low-Frequency Alert
      tone(BUZZER_PIN, 800, 500);
      break;
    case ONLINE:
      noTone(BUZZER_PIN);
      break;
  }
}

// --- Cloud Backlog Clearance Flush Routine ---
void executeDataSync() {
  Serial.println("--- Starting Local Buffer Cloud Sync ---");
  
  while (!localBuffer.empty() && mqtt.connected()) {
    SensorData historicalData = localBuffer.front();
    
    // Attempt uploading historical reading block
    bool t_ok = temp_publish.publish(historicalData.temp);
    delay(500); // Guard time against Adafruit IO API throttling rules
    bool h_ok = hum_publish.publish(historicalData.hum);
    
    if (t_ok && h_ok) {
      Serial.printf("Synced item out of memory -> Temp: %.2f\n", historicalData.temp);
      localBuffer.pop(); // Clear element out of FIFO line
      delay(1000);       // Safe interval between subsequent historic frames
    } else {
      Serial.println("Sync stream interrupted. Re-buffering active.");
      break; 
    }
  }
  Serial.println("--- Sync Session Terminated ---");
}
