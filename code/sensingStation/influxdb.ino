
int writeMeasures(InfluxDBClient &client_idb, Point &pointDevice) {
  pointDevice.clearFields();
  
  pointDevice.addField("rssi", rssiMeasures.getAverage());
  
  if(tempStatus)
    pointDevice.addField("temperature", temperatureMeasures.getAverage());
    
  if(humidStatus)
    pointDevice.addField("humidity", humidityMeasures.getAverage());
    
  if(tempStatus && humidStatus)
    pointDevice.addField("perc_temperature", heatIndex);
   
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(F("Writing finished "));
}

void writeOnDB(){
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

  writeMeasures(client_idb, pointDevice);
}

void initializeInstance(Point &pointDevice){
  Serial.println("Initializing InfluxDB");
  pointDevice.addTag("device", macAddr);
}
