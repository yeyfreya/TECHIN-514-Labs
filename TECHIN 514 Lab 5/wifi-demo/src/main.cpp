#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <stdlib.h>
#include <Wire.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;  // Adjust according to your timezone, in seconds

const char* ssid = "UW MPSK";
const char* password = "Ei9<usFd6T"; // Replace with your network password
const float distanceThreshold = 20.0;

unsigned long lastSendTime = 0; // Last time data was sent
bool objectDetected = false;

#define DATABASE_URL "https://esp32-firebase-514-power-lab-default-rtdb.firebaseio.com/" // Replace with your database URL
#define API_KEY "AIzaSyBw5SdrmLwVpVX6Afc97UzmTG4OGRNe788" // Replace with your API key

#define MAX_WIFI_RETRIES 5   // Maximum number of WiFi connection retries

int uploadInterval = 10000; // 10 seconds each upload
int timeThreshold = 30000;

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

// HC-SR04 Pins
const int trigPin = D2;
const int echoPin = D1;

// Define sound speed in cm/usec
const float soundSpeed = 0.034;

// Function prototypes
float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup()
{
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  connectToWiFi();
  initFirebase();
  // Initialize time
  configTime(gmtOffset_sec, 0, ntpServer);
}

void loop()
{
  unsigned long currentMillis = millis();
  float distance = measureDistance(); // Corrected type to float for distance
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  if (distance <= 50)
  {
    objectDetected = true;
    if (currentMillis - lastSendTime >= timeThreshold)
    {
      // Send data to Firebase
      sendDataToFirebase(distance);
      lastSendTime = currentMillis; // Update last send time
    }
  }
  // Check if current time is outside of 5 AM to 9 PM
  else if (timeinfo.tm_hour < 5 || timeinfo.tm_hour >= 21)
  {
    Serial.print("Current hour: ");
    Serial.println(timeinfo.tm_hour);
    Serial.println("It's between 9 PM and 5 AM, going to sleep.");
    // Calculate sleep time until 5 AM
    unsigned long sleepTime = (24 - timeinfo.tm_hour + 5) % 24 * 3600 * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleepTime); // Set the sleep timer
    esp_deep_sleep_start(); // Enter deep sleep mode
  }
  else
  {
    // If no object detected, check if it's been more than 30 seconds since last detection
    if (objectDetected && (currentMillis - lastSendTime >= 10000))
    {
      Serial.println("No object detected for 30 seconds, going to sleep.");
      // No need to send data to Firebase here as condition for sleep is based on no detection
      esp_sleep_enable_timer_wakeup(30 * 1000000); // Set the sleep timer to 30 seconds
      esp_deep_sleep_start(); // Enter deep sleep mode
    }
  }
  delay(1000); // Delay to avoid flooding with measurements
}

float measureDistance()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi()
{
  Serial.println(WiFi.macAddress());
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi");
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES)
    {
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("Firebase sign-up OK");
    signupOK = true;
  }
  else
  {
    Serial.printf("Firebase sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance)
{
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > uploadInterval || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.pushFloat(&fbdo, "lab5/new", distance))
    {
      Serial.println("Data sent to Firebase");
      Serial.print("PATH: ");
      Serial.println(fbdo.dataPath());
      Serial.print("TYPE: ");
      Serial.println(fbdo.dataType());
    }
    else
    {
    Serial.println("Failed to send data to Firebase");
    Serial.print("REASON: ");
    Serial.println(fbdo.errorReason());
    }
    count++;
  }
}
