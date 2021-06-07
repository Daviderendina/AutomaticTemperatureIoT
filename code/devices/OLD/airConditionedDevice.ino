 #include "airConditionedDescription.h"

 #define AC_UPDATE_TIME 1800000

// MQTT Topics for communicating with network
String AC_TOPIC_TEMPERATUREHUMIDITY = "";   // Receive temperature and humidity update on this topic
String AC_TOPIC_THRESHOLD_HUMID = "";       // Receive update on humidity threshold
String AC_TOPIC_THRESHOLD_TEMPMED = "";     // Receive update on temperature medium threshold
String AC_TOPIC_THRESHOLD_TEMPHIGH = "";    // Receive update on temperature high threshold
String AC_TOPIC_STATUS_CHANGE = "";         // Communicate status of this device

// InfluxDB data
String AC_INFLUXDB_URL = "";
String AC_INFLUXDB_TOKEN = "";
String AC_INFLUXDB_ORG = "";
String AC_INFLUXDB_BUCKET = "";
String AC_POINT_DEVICE = "";

// Flags used for checking discovery protocol status
boolean discoveryStartedAC = false;
boolean discoveryEndedAC = false;

// Threshold of this device
float AC_TEMPERATURE_MED = AC_TEMPERATURE_MED_DEFAULT;
float AC_TEMPERATURE_HIGH = AC_TEMPERATURE_HIGH_DEFAULT;
int   AC_HUMID_HIGH = AC_HUMID_HIGH_DEFAULT;

boolean airConditionedon = false;

String macAddressAC = "";

MQTTClient mqttClientAC(2048);
WiFiClient networkClientAC;


void setupAirConditioned(){
  mqttClientAC.begin(MQTT_BROKERIP, 1883, networkClientAC);   // setup communication with MQTT broker
  mqttClientAC.onMessage(mqttMessageReceivedAC);              // callback on message received from MQTT broker

  macAddressAC = WiFi.macAddress() + "-AC";
}

void loopAirConditioned(){ 
  static unsigned long timeTemp = 0;
  
  checkAndConnectWifi();

  connectToMQTTBrokerAC();   // connect to MQTT broker (if not already connected)
  mqttClientAC.loop();       // MQTT client loop


  int now = millis();
  if(discoveryEndedAC && now - timeTemp > AC_UPDATE_TIME){
    logStatusChangeInfluxAC();
    timeTemp = millis();
  }

  // Start discovery if the process is not finished
  if(!discoveryStartedAC && !discoveryEndedAC)
    startMQTTDiscoveryAC();


}

void startMQTTDiscoveryAC(){
  Serial.println("AC: starting discovery procedure");
  discoveryStartedAC = true;

  // Replace threshold value in the JSon description
  String description = AC_DESCRIPTIon;
  description.replace("@HHIGH@", String(AC_HUMID_HIGH));
  description.replace("@THIGH@", String(AC_TEMPERATURE_HIGH));
  description.replace("@TMED@", String(AC_TEMPERATURE_MED));
  description.replace("@MAC@", String(macAddressAC));

  mqttClientAC.publish(TOPIC_DISCOVERY, description);
}

void mqttMessageReceivedAC(String &topic, String &payload) {
  Serial.println("AC: Incoming MQTT message: " + topic + " - " + payload);

  if(checkTopics(topic, TOPIC_DISCOVERY_RESPONSE) && !discoveryEndedAC)
    handleDiscoveryResponseAC(payload);

  if(checkTopics(topic, AC_TOPIC_TEMPERATUREHUMIDITY))  
    handleNewTemperatureHumidityValueAC(payload);

  if(checkTopics(topic, AC_TOPIC_THRESHOLD_HUMID))  
    handleNewHHighRequestAC(payload);

  if(checkTopics(topic, AC_TOPIC_THRESHOLD_TEMPMED))  
    handleNewTMedRequestAC(payload);

  if(checkTopics(topic, AC_TOPIC_THRESHOLD_TEMPHIGH))  
    handleNewTHighRequestAC(payload);
}

void handleNewHHighRequestAC(String payload){
  Serial.println("AC: new HHIGH threshold value received "+payload);
  AC_HUMID_HIGH = payload.toFloat();
}

void handleNewTMedRequestAC(String payload){
  Serial.println("AC: new TMED threshold value received "+payload);
  AC_TEMPERATURE_MED = payload.toFloat();
}

void handleNewTHighRequestAC(String payload){
  Serial.println("AC: new THIGH threshold value received "+payload);
  AC_TEMPERATURE_HIGH = payload.toFloat();
}

void handleNewTemperatureHumidityValueAC(String payload){
  int humid;
  float temp;
  
  Serial.println("AC: new temperature-humidity values received "+payload);

  char tempStr[3];
  char humidStr[3];
  sscanf(payload.c_str(), "%s %s", &tempStr, &humidStr);

  // If temperature is off, jump everything
  if(tempStr == "off")
    return;
   else
    temp = atof(tempStr);

  if(humidStr == "off")
    humid = -1;
  else
    humid = atoi(humidStr);

  boolean curStatus = airConditionedon;

  if(temp > AC_TEMPERATURE_HIGH){
    airConditionedon = true;
  } else if (temp > AC_TEMPERATURE_MED && humid > AC_HUMID_HIGH){
    airConditionedon = true;
  } else {
    airConditionedon = false;
  }

  if(curStatus != airConditionedon){
    Serial.print(F("AC: air conditioned status changed, current value: "));
    Serial.println((airConditionedon ? "on" : "off"));
    comunicateStatusChangeMQTTAC();
    logStatusChangeInfluxAC();
  }
}

void comunicateStatusChangeMQTTAC(){
  Serial.println("AC: publishing new status on " + AC_TOPIC_STATUS_CHANGE);
  mqttClientAC.publish(AC_TOPIC_STATUS_CHANGE, airConditionedon ? "on" : "off");
}

void logStatusChangeInfluxAC(){
  Serial.println("AC: logging new status to database instance");
  
  InfluxDBClient client_idb(
    AC_INFLUXDB_URL.c_str(), 
    AC_INFLUXDB_ORG.c_str(), 
    AC_INFLUXDB_BUCKET.c_str(), 
    AC_INFLUXDB_TOKEN.c_str());
  Point pointDevice(AC_POINT_DEVICE);

  if(!client_idb.validateConnection()){
    Serial.println("AC: error, cannot reach influxdb instance");
    return;
  }

  pointDevice.addTag("device", macAddressAC);
  pointDevice.addField("airconditioned_status", airConditionedon == true ? 1 : 0);
  
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void handleDiscoveryResponseAC(String payload){

  DynamicJsonDocument response(2048);
  deserializeJson(response, payload);
  const char *mac = response["mac"];

  if(macAddressAC == mac){

    // Read received topics
    JsonObject thr = response["threshold"];
    String tmed = thr["TMED"];
    String thigh = thr["THIGH"];
    String hhigh = thr["HHIGH"];
    AC_TOPIC_THRESHOLD_HUMID = hhigh;
    AC_TOPIC_THRESHOLD_TEMPMED = tmed;
    AC_TOPIC_THRESHOLD_TEMPHIGH = thigh;
    
    JsonObject obs = response["observes"];
    String temphumid = obs["temperaturehumidity"];
    AC_TOPIC_TEMPERATUREHUMIDITY = temphumid;

    String statusChange = response["status"];
    AC_TOPIC_STATUS_CHANGE = statusChange;

    Serial.println("AC: Listening TMED on: "+AC_TOPIC_THRESHOLD_TEMPMED);
    Serial.println("AC: Listening THIGH on: "+AC_TOPIC_THRESHOLD_TEMPHIGH);
    Serial.println("AC: Listening HHIHG on: "+AC_TOPIC_THRESHOLD_HUMID);
    Serial.println("AC: Listening temperature and humidity on: "+AC_TOPIC_TEMPERATUREHUMIDITY);
    Serial.println("AC: Will communicate status on: "+AC_TOPIC_STATUS_CHANGE);
    subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_TEMPMED);
    subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_TEMPHIGH);
    subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_HUMID);
    subscribeSingleTopicAC(AC_TOPIC_TEMPERATUREHUMIDITY);

    JsonObject dbInfo = response["database"];
    String influxUrl = dbInfo["influxdb_url"];
    String influxToken = dbInfo["influxdb_token"];
    String influxOrg = dbInfo["influxdb_org"];
    String infuxBucket = dbInfo["influxdb_bucket"];
    String influxPointDeviceName = dbInfo["influxdb_pointDevice"];
    
    AC_INFLUXDB_URL = influxUrl;
    AC_INFLUXDB_TOKEN = influxToken;
    AC_INFLUXDB_ORG = influxOrg;
    AC_INFLUXDB_BUCKET = infuxBucket;
    AC_POINT_DEVICE = influxPointDeviceName;
    
    discoveryEndedAC = true;
    Serial.println(F("AC: discovery phase finished"));
  }
}


void connectToMQTTBrokerAC() {
  if (!mqttClientAC.connected()) {   // not connected
    Serial.print(F("\nAC: Connecting to MQTT broker..."));
    
    String lastWillMsg = macAddressAC + " off";
    mqttClientAC.setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());
    
    while (!mqttClientAC.connect(AC_MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nAC: Connected!"));

    discoveryStartedAC = false;
    discoveryEndedAC = false;
  }

  mqttClientAC.subscribe(TOPIC_DISCOVERY_RESPONSE);
  subscribeMQTTTopicsAC();
}

void subscribeMQTTTopicsAC(){
  subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_TEMPMED);
  subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_TEMPHIGH);
  subscribeSingleTopicAC(AC_TOPIC_THRESHOLD_HUMID);
  subscribeSingleTopicAC(AC_TOPIC_TEMPERATUREHUMIDITY);
}

void subscribeSingleTopicAC(String topic){
  if(topic != "" && topic != NULL && topic != "null")
    mqttClientAC.subscribe(topic);
}
