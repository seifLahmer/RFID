#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>


LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address of the LCD screen, 16 columns x 2 rows

// WiFi network parameters
const char* ssid = "";
const char* password = "";

// MQTT broker parameters
const char* mqtt_server = "";
const char* mqtt_topic = "test/topic";
const char* mqtt_access_topic = "test/access";


// RFID pins declaration
#define SS_PIN_1 D8
#define RST_PIN_1 D0
#define RED_LED_PIN D1
#define GREEN_LED_PIN D2

MFRC522 mfrc522_1(SS_PIN_1, RST_PIN_1); // Instance for the first RFID module

WiFiClient espClient;
PubSubClient client(espClient);

String content = ""; // Variable to store the UID

// LED control variables
bool redLedOn = false;
bool greenLedOn = false;
unsigned long lastLedChangeTime = 0;
unsigned long lastCardReadTime = 0; // Time of the last RFID card read
const unsigned long cardReadInterval = 5000; // Interval between RFID card reads in milliseconds (5 seconds)

// LCD message reset variables
unsigned long lastMessageUpdateTime = 0;
const unsigned long messageResetInterval = 5000; // Interval to reset the message in milliseconds (5 seconds)

void setup() {
  Serial.begin(9600); // Start serial communication
  SPI.begin(); // Initialize SPI bus
  mfrc522_1.PCD_Init(); // Initialize the first RFID module

  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  setup_wifi(); // Connect to WiFi network
  client.setServer(mqtt_server, 1883); // Initialize MQTT client with broker IP address
  client.setCallback(callback); // Set the callback function for MQTT messages
  client.subscribe(mqtt_access_topic); 
  lcd.init();                       // Initialize the LCD screen
  lcd.backlight();                  // Turn on LCD backlight
  lcd.clear();                      // Clear the LCD screen
  lcd.setCursor(0, 0);              // Set cursor to top left corner
  lcd.print("Pass your card");   

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Check if enough time has passed since the last RFID card read
  unsigned long currentMillis = millis();
  if (currentMillis - lastCardReadTime >= cardReadInterval) {
    // Perform RFID card read
    if (mfrc522_1.PICC_IsNewCardPresent() && mfrc522_1.PICC_ReadCardSerial()) {
      printUID(mfrc522_1.uid, client); // Display UID and send to MQTT broker
      lastCardReadTime = currentMillis; // Update last card read time
    }
  }

  // Check if it's time to reset the message on the LCD
  if (currentMillis - lastMessageUpdateTime >= messageResetInterval) {
    lcd.clear();
      lcd.setCursor(0, 0);              // Set cursor to top left corner
  lcd.print("Pass your card");  
    lastMessageUpdateTime = currentMillis; // Update the last message update time
  }

  // LED control
  controlLEDs();
}

// WiFi connection function
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi network ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("Connected");
      client.subscribe(mqtt_access_topic); // Subscribe to the access authorization topic after reconnection
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5 seconds");
      delay(5000); // Wait 5 seconds before retrying connection
    }
  }
}

// MQTT message callback function
void callback(char* topic, byte* payload, unsigned int length) {
  lcd.clear(); // Clear the LCD screen
  lcd.setCursor(0, 0); // Set cursor to top left corner

  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // Check the content of the received message
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message content: ");
  Serial.println(message);

  // Display the message on the LCD screen
  lcd.print("Message received:");
  lcd.setCursor(0, 1); // Set cursor to line 2
  lcd.print(message);
   
  lastMessageUpdateTime = millis(); // Update the last message update time
}

// Function to display UID and send to MQTT broker
void printUID(MFRC522::Uid uid, PubSubClient &client) {
  content = ""; // Reset the content variable

  for (byte i = 0; i < uid.size; i++) {
    content += String(uid.uidByte[i] < 0x10 ? "0" : ""); // Add leading zero if byte is less than 0x10
    content += String(uid.uidByte[i], DEC); // Convert byte to decimal and add to content
  }
  content += ",";
  content += String(ESP.getChipId());
  Serial.println("UID content: " + content); // Print UID content to serial monitor

  // Send content to MQTT
  if (client.publish(mqtt_topic, content.c_str())) { // Send content to the defined topic
    Serial.println("Message sent to MQTT successfully!");
  } else {
    Serial.println("Failed to send MQTT message!");
  }
}

// Function to control LEDs
void controlLEDs() {
  unsigned long currentTime = millis();

  // Check if LEDs should be turned off after 5 seconds
  if ((redLedOn || greenLedOn) && currentTime - lastLedChangeTime >= 2000) {
    redLedOn = false;
    greenLedOn = false;
  }

  // LED control
  digitalWrite(RED_LED_PIN, redLedOn ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, greenLedOn ? HIGH : LOW);
}
