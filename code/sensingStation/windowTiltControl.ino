boolean deviceOn = false;

void handleDeviceStatusChange(String payload){
  if (payload == "on") {
    deviceOn = true;
    Serial.println("Temperature device ON");
  } else if (payload == "off") {
    deviceOn = false;
    Serial.println("Temperature device OFF");
  } else {
    Serial.println(F("MQTT Payload not recognized, message skipped"));
  }

  checkNewWindowControlStatus();
}

void handleDeviceTiltUpdate(String payload){
  if (payload == "open") {
    openWindowCount++;
    Serial.println("Opened window (" + String(openWindowCount) + " now open)");
  } else if (payload == "close") {
    if(openWindowCount > 0)
      openWindowCount--;
    Serial.println("Closed window (" + String(openWindowCount) + " now open)");
  } else {
    Serial.println(F("MQTT Payload not recognized, message skipped"));
  }

  instantUpdate = true;

  updateLCD();
  checkNewWindowControlStatus();
}

void checkNewWindowControlStatus(){
  boolean actualStatus = deviceOn && (openWindowCount > 0);
  
  // Status changed
  if(windowAlert != actualStatus){
    
    windowAlert = actualStatus;
    writeOnDB("alert");
    communicateAlertMQTT();
    digitalWrite(ALERT_LED, windowAlert ? LOW : HIGH);   
  }
}
