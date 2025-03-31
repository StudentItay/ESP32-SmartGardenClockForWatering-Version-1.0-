#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  // שינוי מ-WiFiClient ל-WiFiClientSecure
#include <PubSubClient.h>
#include <ArduinoJson.h>

// הגדרת חיבורים
#define DHTPIN 4         // חיבור החיישן ל-GPIO 4
#define DHTTYPE DHT22    // סוג החיישן
#define EC_SENSOR_PIN 34 // פין אנלוגי לחיישן EC (מדומה כפוטנציומטר)
#define PH_SENSOR_PIN 35 // פין אנלוגי לחיישן PH (מדומה כפוטנציומטר)

// הגדרת פיני נוריות (LEDs)
int leds[] = {13, 12, 14, 27, 26, 25, 33, 32};
int numLeds = sizeof(leds) / sizeof(leds[0]); // מספר הנוריות

// הגדרות WiFi
const char* ssid = "Wokwi-GUEST";         // שם הרשת שלך
const char* password = "";  // סיסמת הרשת שלך

// הגדרות MQTT מאובטח
const char* mqtt_server = "broker.hivemq.com";  // שרת MQTT מאובטח
const int mqtt_port = 8883;  // פורט TLS/SSL רגיל ל-MQTT
const char* mqtt_user = "";           // שם משתמש MQTT (אופציונלי)
const char* mqtt_password = "";       // סיסמת MQTT (אופציונלי)
const char* mqtt_client_id = "ESP32SecureMQTT";  // שם לקוח

// נושאי MQTT
const char* topic_sensor_data = "esp32/sensor_data";
const char* topic_status = "esp32/status";

// משתנים גלובליים
unsigned long lastMqttPublishTime = 0;
const long mqttPublishInterval = 10000;  // פרסום כל 10 שניות

// אובייקטים
DHT dht(DHTPIN, DHTTYPE);                // יצירת אובייקט DHT
LiquidCrystal_I2C lcd(0x27, 20, 4);      // יצירת אובייקט LCD
RTC_DS1307 rtc;                           // יצירת אובייקט למודול RTC
WiFiClientSecure espClient;               // יצירת אובייקט WiFi מאובטח
PubSubClient mqttClient(espClient);       // יצירת אובייקט MQTT קליינט

void setup_wifi() {
  delay(10);
  // חיבור לרשת WiFi
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
  // הגדרת מצב לא מאובטח - מצפין תעבורה אך לא מאמת את השרת
  // זו אפשרות פשוטה יותר עבור שירות ציבורי כמו HiveMQ
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
  // התחברות לשרת MQTT מאובטח
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT TLS connection...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting MQTT");
    lcd.setCursor(0, 1);
    lcd.print("with TLS...");
    
    // ניסיון התחברות
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      lcd.setCursor(0, 2);
      lcd.print("Connected!");
      
      // פרסום הודעת סטטוס
      mqttClient.publish(topic_status, "ESP32 secure sensor node online");
      
      // הרשמה לנושאים (אם יש צורך לקבל פקודות)
      // mqttClient.subscribe("esp32/commands");
      
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
  // פונקציה זו מופעלת כאשר מתקבלת הודעה מנושא שהרשמנו אליו
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // כאן אפשר להוסיף לוגיקה לטיפול בפקודות שמתקבלות
}

void setup() {
  Serial.begin(115200); // התחלת חיבור סידורי

  dht.begin();  // אתחול חיישן DHT
  lcd.init();   // אתחול LCD
  lcd.backlight(); 

  // אתחול של הנוריות
  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
  }

  if (!rtc.begin()) {
    Serial.println("לא ניתן לאתחל את ה-RTC");
    while (1);
  }

  lcd.setCursor(0, 0);
  lcd.print("System initializing");
  delay(1000);
  
  // הגדרת חיבור WiFi
  setup_wifi();
  
  // הגדרת אבטחת SSL/TLS
  setup_secure_mqtt();
  
  // הגדרת שרת MQTT מאובטח וקולבק
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
  // יצירת JSON עם נתוני החיישנים
  StaticJsonDocument<256> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["ec"] = ec;
  doc["ph"] = ph;
  
  // הוספת מידע על זמן
  DateTime now = rtc.now();
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  doc["time"] = timeStr;
  
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  
  // שליחת הנתונים ל-MQTT
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
  // בדיקת חיבור לשרת MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();
  } else {
    // נסה להתחבר מחדש ל-WiFi אם החיבור נפל
    setup_wifi();
  }

  // קריאת נתונים מהחיישן DHT
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // קריאת נתונים מחיישן EC
  int ecReading = analogRead(EC_SENSOR_PIN);
  float ecVoltage = ecReading * (3.3 / 4095.0);  // ESP32 משתמש ברזולוציה של 12 ביט (0-4095)
  float ecValue = ecVoltage * 1000;  // המרת מתח לערך EC (הערכה גסה, תלוי בחיישן)

  // קריאת נתונים מחיישן pH
  int phReading = analogRead(PH_SENSOR_PIN);
  float phVoltage = phReading * (3.3 / 4095.0);  // ESP32 משתמש ברזולוציה של 12 ביט (0-4095)
  float phValue = 3.5 * phVoltage + 0.5;  // המרת מתח לערך pH (הערכה גסה, דורש כיול)

  // קריאת זמן מה-RTC
  DateTime now = rtc.now();
  // הצגת השעה בשורה הראשונה
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());

  // הצגת נתוני EC
  lcd.setCursor(0, 1);
  lcd.print("EC: ");
  lcd.print(ecValue);
  lcd.print(" mS/cm   ");

  // הצגת נתוני pH
  lcd.setCursor(10, 1); // מיקום מותאם על הצג
  lcd.print("PH: ");
  lcd.print(phValue);
  lcd.print("  ");

  // הצגת נתוני טמפרטורה ולחות
  if (isnan(temp) || isnan(hum)) {
    lcd.setCursor(0, 2);
    lcd.print("Failed to read DHT");
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print(" C  ");
    
    // הצגת לחות בשורה השלישית או בהמשך שורה 2
    lcd.setCursor(15, 2);
    lcd.print("H:");
    lcd.print(hum);
    lcd.print("%");
  }

  // הדפסת נתונים לסידורי
  Serial.print("EC Voltage: ");
  Serial.print(ecVoltage);
  Serial.print("V | EC Value: ");
  Serial.print(ecValue);
  Serial.println(" mS/cm");

  Serial.print("pH Voltage: ");
  Serial.print(phVoltage);
  Serial.print("V | pH Value: ");
  Serial.println(phValue);

  // הפעלת הנוריות לפי טמפרטורה
  activateLedsBasedOnTemperature(temp);

  // שליחת נתונים ל-MQTT בפרקי זמן קבועים
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttPublishTime >= mqttPublishInterval) {
    lastMqttPublishTime = currentMillis;
    
    if (mqttClient.connected() && !isnan(temp) && !isnan(hum)) {
      publish_sensor_data(temp, hum, ecValue, phValue);
    }
  }

  delay(2000);  // המתנה לפני קריאה נוספת
}

void activateLedsBasedOnTemperature(float temp) {
  // כיבוי כל הנוריות לפני הפעלת הנוריות המתאימות
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], LOW);
  }
  
  // הפעלת נוריות על פי טמפרטורה - מירוק לאדום
  int numLedsToLight = 0;
  
  if (temp < 15) {
    numLedsToLight = 1;         // ירוק - קר מאוד
  } else if (temp >= 15 && temp < 24) {
    numLedsToLight = 2;         // ירוק עם קצת צהוב - קר
  } else if (temp >= 24 && temp < 32) {
    numLedsToLight = 3;         // צהוב - טמפרטורה נוחה
  } else if (temp >= 32 && temp < 40) {
    numLedsToLight = 4;         // צהוב עם קצת אדום - חם
  } else if (temp >= 40 && temp < 48) {
    numLedsToLight = 5;         // אדום - חם מאוד  
  } else {
    numLedsToLight = numLeds;   // כל הנוריות - חם מאוד מאוד
  }
  
  // הדלקת הנוריות המתאימות
  for (int i = 0; i < numLedsToLight; i++) {
    digitalWrite(leds[i], HIGH);
  }
}