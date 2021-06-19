/*
The aim of this file is to simulate three different devices connected to the smart network: heating thermostat, air conditioned and a tilt sensor.
This is done by calling one function loop() and setup() for each device. Each device has its own objects. Every macAddress is simulated with a little change of original macAddress.

This file also show the values of the alert and status of the device. This is not necessary for the project (for example in a real application) but it's useful during our simulation,
because we can show the change of status and values in photos and videos.
*/

 #include "HeatingDevice.h" 
 #include "AirConditionedDevice.h"
 #include "TiltSensor.h"
 #include "AlertDevice.h" 

 HeatingDevice *heating;
 AirConditionedDevice *ac;
 TiltSensor *tiltSensor;
 AlertDevice *al;

// SIMULATION
#include <LiquidCrystal_I2C.h>  // display library
bool acStatus = false, htStatus = false;
#define DISPLAY_ADDR 0x27   // display address on I2C bus
LiquidCrystal_I2C lcd(DISPLAY_ADDR, 16, 2);


void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);

  heating = new HeatingDevice();
  ac = new AirConditionedDevice();
  tiltSensor = new TiltSensor();
  al = new AlertDevice();
  
  heating->setup();
  ac->setup();
  tiltSensor->setup();
  al->setup();

  simulationSetup();
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void loop() {
  
  heating->loop();
  ac->loop();
  tiltSensor->loop();
  al->loop();

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
}

void simulationLoop(){
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
