#include "secrets.h"            

#include <LiquidCrystal_I2C.h>  // display library
#include <Wire.h>               // I2C library
#include <DHT.h>
#include <InfluxDbClient.h> // InfluxDB library
#include <MQTT.h>
#include <ESP8266WiFi.h>        // Wifi
#include <ArduinoJson.h>

#include "MeasureArrayHandler.h"

// Pin mapping
#define DHTPIN D3   
#define TILT D5       
#define ALERT_LED D6
#define CRIT_LED D7  

// Costant for hardware
#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27   // display address on I2C bus
#define DHTTYPE DHT11   // sensor type DHT 11
#define BUTTON_DEBOUNCE_DELAY 20  // Button debounce

// Software constants
#define TIME_UPDATE_NETWORK         1800000   // Update and check every 30min
#define TIME_UPDATE_MEASURES_ARRAY   300000    // Read value every 5 minutes
#define TIME_CHECK_TILT     400    // Check tilt value change every 400 milliseconds
#define SIZE_MEASURES_AVG_ARRAY    6         // Store last 6 values measured


// Initialize objects for sensors
DHT dht = DHT(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object

// MQTT Communication
MQTTClient mqttClient(2048);                    
WiFiClient networkClient;

// Discovery flags
boolean discoveryStarted = false;
boolean discoveryEnded = false;

// InfluxDB parameters
String INFLUXDB_URL = "";
String INFLUXDB_TOKEN = "";
String INFLUXDB_ORG = "";
String INFLUXDB_BUCKET = "";
String POINT_DEVICE = "";

// WiFi configuration
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Sensor status (ON/OFF) flags
boolean tempStatus = true;
boolean humidStatus = true;
boolean lcdStatus = true;
boolean tiltStatus = true;

// Flag to force values update
boolean instantUpdate = false;

// Global variables for values
MeasureArrayHandler temperatureMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler humidityMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler rssiMeasures(SIZE_MEASURES_AVG_ARRAY);
float heatIndex;
byte tiltValue;
String macAddr;

// Variables used in windowTiltControl
boolean windowAlert = false;
int openWindowCount = 0;


void setup() {
  Serial.begin(115200);

  mqttSetup();
  WiFi.mode(WIFI_STA);
  
  setupLCD();
  dht.begin();
  
  pinMode(TILT, INPUT_PULLUP);

  pinMode(ALERT_LED, OUTPUT);
  digitalWrite(ALERT_LED, HIGH);
  
  pinMode(CRIT_LED, OUTPUT);
  digitalWrite(CRIT_LED, HIGH);
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}


void loop() {
  static unsigned long timeTemp = TIME_UPDATE_NETWORK;
  static unsigned long timeTempTiltCheck = TIME_CHECK_TILT;
  static unsigned long timeTempUpdateMeasures = TIME_UPDATE_MEASURES_ARRAY;
  
  // Check WiFi 
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
    macAddr = WiFi.macAddress();
  }

  // MQTT
  connectToMQTTBrokerAndSubscribe();   
  mqttClient.loop();       

  if(!discoveryStarted && !discoveryEnded)
    performDiscovery();

  if(discoveryEnded){

    // Check every 500ms if tilt status change
    if(millis() - timeTempTiltCheck > TIME_CHECK_TILT || instantUpdate){
      checkTiltChangedAndCommunicateMQTT();

      // Restore auxiliary values
      timeTempTiltCheck = millis();
      instantUpdate = false;
    }
    
    // Read values from sensors and update measure array
    if(millis() - timeTempUpdateMeasures > TIME_UPDATE_MEASURES_ARRAY || instantUpdate){
      if(tempStatus)
        readTemperatureDHT();
  
      if(humidStatus)
        readHumidityDHT();

      int rssiValue= WiFi.RSSI();
      rssiMeasures.addElement(rssiValue);

      // Restore auxiliary values
      timeTempUpdateMeasures = millis();
      instantUpdate = false;
    }
    
    // Update MQTT and InfluxDB
    if(millis() - timeTemp > TIME_UPDATE_NETWORK || instantUpdate){ 
      if(humidStatus && tempStatus)
        calculateHeatIndex();
  
      if(tiltStatus){
        readTiltValue();
      } else {
        tiltValue = LOW;
      }
  
      // Update LCD
      if(lcdStatus){
        lcd.setBacklight(255);
        updateLCD();
      }
      else{
        lcd.setBacklight(0);
      }

      // Write temperature and humidity on MQTT
      communicateValuesMQTT(temperatureMeasures.getAverage(), humidityMeasures.getAverage());

      // Update influxDB instance
      writeOnDB("measures");
      
      timeTemp = millis();
      instantUpdate = false;
      Serial.println(F("-----------------------------"));
    }
  }
}

void checkTiltChangedAndCommunicateMQTT(){
  byte newTiltValue = digitalRead(TILT);
  if(newTiltValue != tiltValue){
    Serial.println(F("Found different tilt status: sending MQTT message"));
    tiltValue = newTiltValue;
    
    publishTiltMQTT();
  }
}

void readTemperatureDHT(){
  float tempValue = dht.readTemperature();
  Serial.println("Temp: " + String(tempValue) +"C");
  temperatureMeasures.addElement(tempValue);
}

void readHumidityDHT(){
  float humidValue = dht.readHumidity();
  Serial.println("Humidity: " + String(humidValue) +"%");
  humidityMeasures.addElement(humidValue);
}

void calculateHeatIndex(){
  heatIndex = dht.computeHeatIndex(temperatureMeasures.getAverage(), humidityMeasures.getAverage(), false);
  Serial.println("PercTemp: " + String(heatIndex) +"C");
}

void readTiltValue(){
  tiltValue = digitalRead(TILT);
  Serial.print(F("Tilt value: "));
  Serial.println(tiltValue == HIGH ? "HIGH" : "LOW");
}

void connectWifi(){  
  digitalWrite(CRIT_LED, LOW);
  Serial.print(F("Connecting to SSID: "));
  Serial.println(SECRET_SSID);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(250);
  }
  Serial.print(F("\nConnected to: "));
  Serial.print(WiFi.SSID());
  Serial.print(F(" - My IP: "));
  Serial.println(WiFi.localIP());
  digitalWrite(CRIT_LED, HIGH);
}
