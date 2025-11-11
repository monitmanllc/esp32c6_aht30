#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ===== CONFIGURATION =====
// WiFi Credentials
const char* ssid = "SomeWifiNetwork";
const char* password = "BalloonAnimalsRule12";

// Server Configuration
const char* serverUrl = "https://app.monitman.com/dashboard/receive.php?sensor=tah";  // Change to your server URL

// AHT30 I2C address
#define AHT30_ADDRESS 0x38

// AHT30 commands
#define AHT30_INIT_CMD 0xBE
#define AHT30_TRIGGER_CMD 0xAC
#define AHT30_SOFT_RESET 0xBA

// Timing
unsigned long lastPostTime = 0;
const unsigned long postInterval = 60000; // 60 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nAHT30 WiFi Data Logger");
  
  // Initialize I2C
  Wire.begin(6, 7); // SDA=GPIO6, SCL=GPIO7 (adjust for your ESP32-C6 pins)
  delay(100);
  
  // Initialize AHT30
  if (initAHT30()) {
    Serial.println("AHT30 initialized successfully");
  } else {
    Serial.println("Failed to initialize AHT30");
  }
  
  // Connect to WiFi
  connectWiFi();
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
  }
  
  // Post data every minute
  if (millis() - lastPostTime >= postInterval) {
    lastPostTime = millis();
    
    float temperature, humidity;
    if (readAHT30(&temperature, &humidity)) {
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      postData(temperature, humidity);
    } else {
      Serial.println("Failed to read from AHT30");
    }
  }
  
  delay(1000);
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

bool initAHT30() {
  // Soft reset
  Wire.beginTransmission(AHT30_ADDRESS);
  Wire.write(AHT30_SOFT_RESET);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(20);
  
  // Initialize
  Wire.beginTransmission(AHT30_ADDRESS);
  Wire.write(AHT30_INIT_CMD);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(10);
  
  return true;
}

bool readAHT30(float* temperature, float* humidity) {
  // Trigger measurement
  Wire.beginTransmission(AHT30_ADDRESS);
  Wire.write(AHT30_TRIGGER_CMD);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  // Wait for measurement to complete
  delay(80);
  
  // Read 7 bytes of data
  Wire.requestFrom(AHT30_ADDRESS, 7);
  
  if (Wire.available() < 7) {
    return false;
  }
  
  uint8_t data[7];
  for (int i = 0; i < 7; i++) {
    data[i] = Wire.read();
  }
  
  // Check if busy bit is clear
  if (data[0] & 0x80) {
    return false;
  }
  
  // Extract humidity (20 bits)
  uint32_t rawHumidity = ((uint32_t)data[1] << 12) | 
                         ((uint32_t)data[2] << 4) | 
                         ((uint32_t)data[3] >> 4);
  
  // Extract temperature (20 bits)
  uint32_t rawTemperature = (((uint32_t)data[3] & 0x0F) << 16) | 
                            ((uint32_t)data[4] << 8) | 
                            (uint32_t)data[5];
  
  // Convert to actual values
  *humidity = (rawHumidity * 100.0) / 1048576.0;
  *temperature = ((rawTemperature * 200.0) / 1048576.0) - 50.0;
  
  return true;
}

void postData(float temperature, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping POST");
    return;
  }
  
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Build JSON string manually
  String jsonData = "{";
  jsonData += "\"temperature\":";
  jsonData += String(temperature, 2);
  jsonData += ",\"humidity\":";
  jsonData += String(humidity, 2);
  jsonData += ",\"sensor\":\"AHT30\"";
  jsonData += ",\"timestamp\":";
  jsonData += String(millis());
  jsonData += "}";
  
  Serial.println("Posting data: " + jsonData);
  
  int httpResponseCode = http.POST(jsonData);
  
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.printf("Error on HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
}
