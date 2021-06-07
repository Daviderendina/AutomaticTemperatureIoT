void setupLCD(){
  Serial.println(F("\n\nChecking LCD connection..."));
  
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);
  byte error = Wire.endTransmission();

  if (error == 0) {
    Serial.println(F("LCD found."));
    lcd.begin(DISPLAY_CHARS, 2);   // initialize the lcd

  } else {
    Serial.print(F("LCD not found. Error "));
    Serial.println(error);
    Serial.println(F("Check connections and configuration. Reset to try again!"));
    while (true){
      Serial.println(F("Delay 1 - error"));
      delay(1);
    }
  }

  lcd.setBacklight(255);    // set backlight to maximum
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Welcome");   // show text

  Serial.println(F("\n\nLCD connection OK"));
}


// LCD
void updateLCD(){ 
  printSensorValuesLCD(
    getPrintString(temperatureMeasures.getAverage(), tempStatus),
    getPrintString(humidityMeasures.getAverage(), humidStatus),
    getPrintString(rssiMeasures.getAverage(), WiFi.status() == WL_CONNECTED),
    tiltStatus ? String(openWindowCount) : "OFF");
}

String getPrintString(int value, boolean sensorStatus){
  return sensorStatus ? 
    (isnan(value) ? "ERR" : String(value))
    : "OFF"; 
}

boolean needUnitOfMeasure(String str){
  return ! (str.equals("ERR") || str.equals("OFF"));
}

void printSensorValuesLCD(String temp_str, String humid_str, String rssi_str, String tiltStr){
  
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("U: ");
  lcd.print(humid_str);
  if(needUnitOfMeasure(humid_str))
    lcd.print("%");

  lcd.print("   T: ");
  lcd.print(temp_str);
  if(needUnitOfMeasure(temp_str))
    lcd.print("C");

  
  lcd.setCursor(0, 1);
  
  lcd.print("R: ");   
  lcd.print(rssi_str);

  lcd.print("   W: ");
  lcd.print(tiltStr);
}
