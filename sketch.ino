#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Connection definitions
#define DHTPIN 4         // Connecting the sensor to GPIO 4
#define DHTTYPE DHT22    // Temperature sensor
#define EC_SENSOR_PIN 34 // Analog pin for EC sensor (simulated as potentiometer)
#define PH_SENSOR_PIN 35 // Analog pin for PH sensor (simulated as potentiometer)

// LED pin definitions
int leds[] = {13, 12, 14, 27, 26, 25, 33, 32};
int numLeds = sizeof(leds) / sizeof(leds[0]); // Number of LEDs

// WiFi settings
const char* ssid = "Wokwi-GUEST";         // Your network name
const char* password = "";  // Your network password

// Secure MQTT settings
const char* mqtt_server = "broker.hivemq.com";  // Secure MQTT server
const int mqtt_port = 8883;  // Regular TLS/SSL port for MQTT
const char* mqtt_user = "";           // MQTT username (optional)
const char* mqtt_password = "";       // MQTT password (optional)
const char* mqtt_client_id = "";  // Client name

// MQTT topics
const char* topic_sensor_data = "esp32/sensor_data";
const char* topic_status = "esp32/status";

// Global variables
unsigned long lastMqttPublishTime = 0;
const long mqttPublishInterval = 10000;  

// Objects
DHT dht(DHTPIN, DHTTYPE);                // Create DHT object
LiquidCrystal_I2C lcd(0x27, 20, 4);      // Create LCD object
RTC_DS1307 rtc;                           // Create RTC module object
WiFiClientSecure espClient;               // Create secure WiFi object
PubSubClient mqttClient(espClient);       // Create MQTT client object

void setup_wifi() {
  delay(10);
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to WiFi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.begin(ssid, password, 6);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempt % 16, 2);
    lcd.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println("Failed to connect to WiFi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi connection");
    lcd.setCursor(0, 1);
    lcd.print("failed!");
    delay(2000);
  }
}

void setup_secure_mqtt() {
  // Setting insecure mode - encrypts traffic but doesn't verify the server
  // This is a simpler option for a public service like HiveMQ
  espClient.setInsecure();
  
  Serial.println("SSL/TLS configured in insecure mode");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SSL/TLS configured");
  lcd.setCursor(0, 1);
  lcd.print("(insecure mode)");
  delay(1000);
}

void reconnect_mqtt() {
  // Connecting to secure MQTT server
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT TLS connection...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting MQTT");
    lcd.setCursor(0, 1);
    lcd.print("with TLS...");
    
    // Connection attempt
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      lcd.setCursor(0, 2);
      lcd.print("Connected!");
      
      // Publishing status message
      mqttClient.publish(topic_status, "ESP32 secure sensor node online");
      

      
      delay(1000);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      
      lcd.setCursor(0, 2);
      lcd.print("Failed! Retrying...");
      Serial.println("Error: " + String(mqttClient.state()));
      delay(1000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // This function is called when a message is received from a topic we subscribed to
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Here you can add logic to handle received commands
}

void setup() {
  Serial.begin(115200); // Start serial connection

  dht.begin();  // Initialize DHT sensor
  lcd.init();   // Initialize LCD
  lcd.backlight(); 

  // Initialize LEDs
  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
  }

  if (!rtc.begin()) {
    Serial.println("Cannot initialize RTC");
    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("System initializing");
  delay(1000);
  
  // Setup WiFi connection
  setup_wifi();
  

  setup_secure_mqtt();
  
  // Setup secure MQTT server and callback
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EC Sensor Ready");
  lcd.setCursor(0, 1);
  lcd.print("PH Sensor Ready");
  delay(2000);
  lcd.clear();
}

void publish_sensor_data(float temp, float hum, float ec, float ph) {
  // Create JSON with sensor data
  StaticJsonDocument<256> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["ec"] = ec;
  doc["ph"] = ph;
  
  // Add time information
  DateTime now = rtc.now();
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  doc["time"] = timeStr;
  
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  
  // Send data to MQTT
  if (mqttClient.publish(topic_sensor_data, jsonBuffer)) {
    Serial.println("MQTT data published successfully");
    lcd.setCursor(0, 3);
    lcd.print("MQTT: OK   ");
  } else {
    Serial.println("MQTT publish failed");
    lcd.setCursor(0, 3);
    lcd.print("MQTT: Error");
  }
}

void loop() {
  // Check connection to MQTT server
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();
  } else {
    // Try to reconnect to WiFi if connection is lost
    setup_wifi();
  }

  // Read data from DHT sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Read data from EC sensor
  int ecReading = analogRead(EC_SENSOR_PIN);
  float ecVoltage = ecReading * (3.3 / 4095.0);  // ESP32 uses 12-bit resolution (0-4095)
  float ecValue = ecVoltage * 1000;  // Convert voltage to EC value (rough estimate, depends on sensor)

  // Read data from pH sensor
  int phReading = analogRead(PH_SENSOR_PIN);
  float phVoltage = phReading * (3.3 / 4095.0);  // ESP32 uses 12-bit resolution (0-4095)
  float phValue = 3.5 * phVoltage + 0.5;  // Convert voltage to pH value (rough estimate, requires calibration)

  // Read time from RTC
  DateTime now = rtc.now();
  // Display time in first row
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());

  // Display EC data
  lcd.setCursor(0, 1);
  lcd.print("EC: ");
  lcd.print(ecValue);
  lcd.print(" mS/cm   ");

  // Display pH data
  lcd.setCursor(10, 1); // Adjusted position on display
  lcd.print("PH: ");
  lcd.print(phValue);
  lcd.print("  ");

  // Display temperature and humidity data
  if (isnan(temp) || isnan(hum)) {
    lcd.setCursor(0, 2);
    lcd.print("Failed to read DHT");
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print(" C  ");
    
    // Display humidity in third row or continuation of row 2
    lcd.setCursor(15, 2);
    lcd.print("H:");
    lcd.print(hum);
    lcd.print("%");
  }

  // Print data to serial
  Serial.print("EC Voltage: ");
  Serial.print(ecVoltage);
  Serial.print("V | EC Value: ");
  Serial.print(ecValue);
  Serial.println(" mS/cm");

  Serial.print("pH Voltage: ");
  Serial.print(phVoltage);
  Serial.print("V | pH Value: ");
  Serial.println(phValue);

  // Activate LEDs based on temperature
  activateLedsBasedOnTemperature(temp);

  // Send data to MQTT at fixed intervals
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttPublishTime >= mqttPublishInterval) {
    lastMqttPublishTime = currentMillis;
    
    if (mqttClient.connected() && !isnan(temp) && !isnan(hum)) {
      publish_sensor_data(temp, hum, ecValue, phValue);
    }
  }

  delay(2000);  // Wait before next reading
}

void activateLedsBasedOnTemperature(float temp) {
  // Turn off all LEDs before activating appropriate ones
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], LOW);
  }
  
  // Activate LEDs based on temperature - from green to red
  int numLedsToLight = 0;
  
  if (temp < 15) {
    numLedsToLight = 1;         // Green - very cold
  } else if (temp >= 15 && temp < 24) {
    numLedsToLight = 2;         // Green with some yellow - cold
  } else if (temp >= 24 && temp < 32) {
    numLedsToLight = 3;         // Yellow - comfortable temperature
  } else if (temp >= 32 && temp < 40) {
    numLedsToLight = 4;         // Yellow with some red - hot
  } else if (temp >= 40 && temp < 48) {
    numLedsToLight = 5;         // Red - very hot  
  } else {
    numLedsToLight = numLeds;   // All LEDs - extremely hot
  }
  
  // Turn on appropriate LEDs
  for (int i = 0; i < numLedsToLight; i++) {
    digitalWrite(leds[i], HIGH);
  }
}