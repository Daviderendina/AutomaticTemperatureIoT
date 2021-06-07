/*
The aim of this file is to simulate three different devices connected to the smart network: heating thermostat, air conditioned and a tilt sensor.
This is done by calling one function loop() and setup() for each device. Each device has its own objects.
- Every macAddress is simulated with a little change of original macAddress.
*/

 #include "HeatingDevice.h" 
 #include "AirConditionedDevice.h"
 #include "TiltSensor.h"

 HeatingDevice *heating;
 AirConditionedDevice *ac;
 TiltSensor *tiltSensor;


void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);

  heating = new HeatingDevice();
  ac = new AirConditionedDevice();
  tiltSensor = new TiltSensor();
  
  heating->setup();
  ac->setup();
  tiltSensor->setup();
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void loop() {
  heating->loop();
  ac->loop();
  tiltSensor->loop();
}
