/*
The aim of this file is to simulate three different devices connected to the smart network: heating thermostat, air conditioned and a tilt sensor.
This is done by calling one function loop() and setup() for each device. Each device has its own objects. Every macAddress is simulated with a little change of original macAddress.

This file also show the values of the alert and status of the device. This is not necessary for the project (for example in a real application) but it's useful during our simulation,
because we can show the change of status and values in photos and videos.
*/

 #include "HeatingDevice.h" 
 #include "AirConditionedDevice.h"
 #include "TiltSensor.h"

 HeatingDevice *heating;
 AirConditionedDevice *ac;
 TiltSensor *tiltSensor;

// SIMULATION
#include <LiquidCrystal_I2C.h>  // display library
MQTTClient mqttClientSimulation(2048);
WiFiClient networkClient;
bool acStatus = false, htStatus = false;
#define DISPLAY_ADDR 0x27   // display address on I2C bus
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 16, 2);


void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);

  heating = new HeatingDevice();
  ac = new AirConditionedDevice();
  tiltSensor = new TiltSensor();
  
  heating->setup();
  ac->setup();
  tiltSensor->setup();

  simulationSetup();
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void loop() {
  
  heating->loop();
  ac->loop();
  tiltSensor->loop();

  simulationLoop();
}

void simulationSetup(){
  // LCD
  Serial.println(F("\n\nChecking LCD connection..."));
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println(F("LCD found."));
    lcd.begin(16, 2);   // initialize the lcd
  } else {
    Serial.print(F("LCD not found. Error "));
    Serial.println(error);
    Serial.println(F("Check connections and configuration. Reset to try again!"));
    while (true){
      delay(1);
    }
  }
  lcd.setBacklight(255);    // set backlight to maximum
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear textlcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Heating: ");
  lcd.print("OFF");
  
  lcd.setCursor(0, 1);  
  lcd.print("AirCond: ");   
  lcd.print("OFF");
  
  Serial.println(F("\n\nLCD connection OK"));
  
  // USED ONLY FOR SHOWING VALUES DURING THE SIMULATION
  mqttClientSimulation.begin(MQTT_BROKERIP, 1883, networkClient);
  mqttClientSimulation.onMessage(simulationMessageReceived);

  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);
}

void simulationLoop(){
  // WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(250);
    }
    Serial.print(F("\nSIMULATION: Connected to: "));
    Serial.print(WiFi.SSID());
  }

  // mqtt Client connection
  if (!mqttClientSimulation.connected()) {   // not connected
    Serial.println(F("SIMULATION: Connecting to MQTT broker..."));
    String clientId = MQTT_CLIENTID + String("simulation");

    while (!mqttClientSimulation.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("SIMULATION: Connected!"));

    mqttClientSimulation.subscribe("rt/alert/windowOpen");
  }
        
  mqttClientSimulation.loop();

  if(htStatus != heating->getStatus() || acStatus != ac->getStatus()){
    htStatus = heating->getStatus();
    acStatus = ac->getStatus();
    
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Heating: ");
    lcd.print(heating->getStatus() ? "ON" : "OFF");
    
    lcd.setCursor(0, 1);  
    lcd.print("AirCond: ");   
    lcd.print(ac->getStatus() ? "ON" : "OFF");
  }
}

void simulationMessageReceived(String &topic, String &payload){
  Serial.println("SIMULATION: msg received "+ payload);
  if(payload == "on"){
    Serial.println("SIMULATION: turn off led");
    digitalWrite(D5, LOW);
  } else if (payload == "off"){
    Serial.println("SIMULATION: turn off led");
    digitalWrite(D5, HIGH);
  }
}
