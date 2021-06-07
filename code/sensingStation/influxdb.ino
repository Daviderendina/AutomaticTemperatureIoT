
int writeMeasures(InfluxDBClient &client_idb, Point &pointDevice) {
  pointDevice.clearFields();
  
  pointDevice.addField("rssi", rssiMeasures.getAverage());
  
  if(tempStatus)
    pointDevice.addField("temperature", temperatureMeasures.getAverage());
    
  if(humidStatus)
    pointDevice.addField("humidity", humidityMeasures.getAverage());
    
  if(tempStatus && humidStatus)
    pointDevice.addField("perc_temperature", heatIndex);
    
  if(tiltStatus){
    Serial.println(openWindowCount);
    pointDevice.addField("open_windows_count", openWindowCount);
    pointDevice.addField("window_alert", windowAlert ? 1 : 0);
  }
  
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(F("Writing finished "));
}

int writeAlert(InfluxDBClient &client_idb, Point &pointDevice) {
  pointDevice.clearFields();
  
  pointDevice.addField("window_alert", windowAlert ? 1 : 0);
  
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void writeOnDB(String type){
  Serial.println("Updating database");
  
  InfluxDBClient client_idb(
    INFLUXDB_URL.c_str(), 
    INFLUXDB_ORG.c_str(), 
    INFLUXDB_BUCKET.c_str(), 
    INFLUXDB_TOKEN.c_str());
  Point pointDevice(POINT_DEVICE);

  if(!client_idb.validateConnection()){
    Serial.println("Error, cannot reach database instance");
    return;
  }
  
  initializeInstance(pointDevice);

  if(type == "measures"){
    writeMeasures(client_idb, pointDevice);
  }

  if(type == "alert")
    writeAlert(client_idb, pointDevice); 
}

void initializeInstance(Point &pointDevice){
  Serial.println("Initializing InfluxDB");
  pointDevice.addTag("device", macAddr);
}
