#include "heatingDescription.h"

#define AC_UPDATE_TIME 1800000

/// MQTT Topics for communicating with network
String HT_TOPIC_TEMPERATURE = "";     // Receive update on temperature value
String HT_TOPIC_THRESHOLD_TON = "";   // Receive update on temperature_on threshold (turn on heating)
String HT_TOPIC_THRESHOLD_TOFF = "";  // Receive update on temperature_off threshold (turn off heating)
String HT_TOPIC_STATUS_CHANGE = "";   // Communicate device status change

// InfluxDB data
String HT_INFLUXDB_URL = "";
String HT_INFLUXDB_TOKEN = "";
String HT_INFLUXDB_ORG = "";
String HT_INFLUXDB_BUCKET = "";
String HT_POINT_DEVICE = "";

// Flags used for checking discovery protocol status
boolean discoveryStartedHT = false;
boolean discoveryEndedHT = false;

// Threshold of this device
float HT_TEMPERATURE_OFF = HT_TOFF_THRESHOLD_DEFAULT;
float HT_TEMPERATURE_ON = HT_TON_THRESHOLD_DEFAULT;

// Flag that indicates if the heating is on or off
boolean heatingOn = false;

String macAddressHT = "";

MQTTClient mqttClientHT(2048);
WiFiClient networkClientHT;


void setupHeating(){
  mqttClientHT.begin(MQTT_BROKERIP, 1883, networkClientHT);   
  mqttClientHT.onMessage(mqttMessageReceivedHT);              

  macAddressHT = WiFi.macAddress() + "-HT";
}

void loopHeating(){
  checkAndConnectWifi();

  connectToMQTTBrokerHT();
  mqttClientHT.loop();       
  
  // Start discovery if the process is not finished
  if(!discoveryStartedHT && !discoveryEndedHT)
    startMQTTDiscoveryHT();

  int now = millis();
  if(discoveryEndedAC && now - timeTemp > AC_UPDATE_TIME){
    logStatusChangeInfluxHT();
    timeTemp = millis();
  }
}

void startMQTTDiscoveryHT(){
  Serial.println("HT: starting discovery procedure");
  discoveryStartedHT = true;

  // Replace threshold value in the JSon description
  String discoveryMessage = HT_DESCRIPTION;
  discoveryMessage.replace("@TON@", String(HT_TEMPERATURE_ON));
  discoveryMessage.replace("@TOFF@", String(HT_TEMPERATURE_OFF));
  discoveryMessage.replace("@MAC@", macAddressHT);

  mqttClientHT.publish(TOPIC_DISCOVERY, discoveryMessage);
}

void mqttMessageReceivedHT(String &topic, String &payload) {
  Serial.println("HT: Incoming MQTT message: " + topic + " - " + payload);

  if(checkTopics(topic, TOPIC_DISCOVERY_RESPONSE) && !discoveryEndedHT)
    handleDiscoveryResponseHT(payload);

  if(checkTopics(topic, HT_TOPIC_TEMPERATURE)) 
    handleNewTemperatureValueHT(payload);
  
  if(checkTopics(topic, HT_TOPIC_THRESHOLD_TON))  
    handleNewTOnRequestHT(payload);
  
  if(checkTopics(topic, HT_TOPIC_THRESHOLD_TOFF))   
    handleNewTOffRequestHT(payload);
}

void handleNewTemperatureValueHT(String payload){
  Serial.println("HT: new temperature value received "+payload);
  float curTemp = payload.toFloat();

  // Calculate current heating status, according to temperature received
  boolean curStatus = heatingOn;
  if(curTemp < HT_TEMPERATURE_ON && ! heatingOn){
    curStatus = true;
  } else if (curTemp > HT_TEMPERATURE_OFF && heatingOn){
    curStatus = false;
  }

  // If the status change, perform status change operations
  if(curStatus != heatingOn){
    heatingOn = curStatus;
    Serial.print(F("HT: heating status changed, current value: "));
    Serial.println((heatingOn ? "on" : "off"));
    comunicateStatusChangeHT();
    logStatusChangeInfluxHT();
  }
}

void comunicateStatusChangeHT(){
  Serial.println("HT: publishing new status on "+HT_TOPIC_STATUS_CHANGE);
  mqttClientHT.publish(HT_TOPIC_STATUS_CHANGE, heatingOn ? "on" : "off");
}

void logStatusChangeInfluxHT(){
  Serial.println("HT: logging new status to database instance");
  
  InfluxDBClient client_idb(
    HT_INFLUXDB_URL.c_str(), 
    HT_INFLUXDB_ORG.c_str(), 
    HT_INFLUXDB_BUCKET.c_str(), 
    HT_INFLUXDB_TOKEN.c_str());
  Point pointDevice(HT_POINT_DEVICE);

  if(!client_idb.validateConnection()){
    Serial.println("HT: error, cannot reach influxdb instance");
    return;
  }

  pointDevice.addTag("device", macAddressHT);
  pointDevice.addField("heating_status", heatingOn == true ? 1 : 0);
  
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void handleNewTOnRequestHT(String payload){
  Serial.println("HT: new TON threshold value received "+payload);
  HT_TEMPERATURE_ON = payload.toFloat();
}

void handleNewTOffRequestHT(String payload){
  Serial.println("HT: new TOFF threshold value received "+payload);
  HT_TEMPERATURE_OFF = payload.toFloat();
}

void handleDiscoveryResponseHT(String payload){
  DynamicJsonDocument description(2048);
  deserializeJson(description, HT_DESCRIPTION);

  DynamicJsonDocument response(2048);
  deserializeJson(response, payload);
  const char *mac = response["mac"];

  if(macAddressHT == mac){

    // Read received topics
    JsonObject thr = response["threshold"];
    String ton = thr["TON"];
    String toff = thr["TOFF"];
    HT_TOPIC_THRESHOLD_TON = ton;
    HT_TOPIC_THRESHOLD_TOFF = toff;
    
    JsonObject obs = response["observes"];
    String temp = obs["temperature"];
    HT_TOPIC_TEMPERATURE = temp;

    String statusChange = response["status"];
    HT_TOPIC_STATUS_CHANGE = statusChange;

    Serial.println("HT: Listening TON on: " + HT_TOPIC_THRESHOLD_TON);
    Serial.println("HT: Listening TOFF on: " + HT_TOPIC_THRESHOLD_TOFF);
    Serial.println("HT: Listening temperature on: " + HT_TOPIC_TEMPERATURE);
    Serial.println("HT: Will communicate status on: " + HT_TOPIC_STATUS_CHANGE);
    subscribeSingleTopicHT(HT_TOPIC_TEMPERATURE);
    subscribeSingleTopicHT(HT_TOPIC_THRESHOLD_TON);
    subscribeSingleTopicHT(HT_TOPIC_THRESHOLD_TOFF);

    JsonObject dbInfo = response["database"];
    String influxUrl = dbInfo["influxdb_url"];
    String influxToken = dbInfo["influxdb_token"];
    String influxOrg = dbInfo["influxdb_org"];
    String infuxBucket = dbInfo["influxdb_bucket"];
    String influxPointDeviceName = dbInfo["influxdb_pointDevice"];
    
    HT_INFLUXDB_URL = influxUrl;
    HT_INFLUXDB_TOKEN = influxToken;
    HT_INFLUXDB_ORG = influxOrg;
    HT_INFLUXDB_BUCKET = infuxBucket;
    HT_POINT_DEVICE = influxPointDeviceName;
    
    discoveryEndedHT = true;
    Serial.println(F("HT: discovery phase finished"));
  }
}


void connectToMQTTBrokerHT() {
  if (!mqttClientHT.connected()) {   // not connected
    Serial.print(F("\nHT: Connecting to MQTT broker..."));
    
    String lastWillMsg = macAddressHT + " off";
    mqttClientHT.setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());
    
    while (!mqttClientHT.connect(HT_MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nHT: Connected!"));

    discoveryStartedHT = false;
    discoveryEndedHT = false;
    
    subscribeMQTTTopicsHT();
  }
}

void subscribeMQTTTopicsHT(){
  subscribeSingleTopicHT(TOPIC_DISCOVERY_RESPONSE);
  subscribeSingleTopicHT(HT_TOPIC_TEMPERATURE);
  subscribeSingleTopicHT(HT_TOPIC_THRESHOLD_TON);
  subscribeSingleTopicHT(HT_TOPIC_THRESHOLD_TOFF);
}

void subscribeSingleTopicHT(String topic){
  if(topic != "" && topic != NULL && topic != "null")
    mqttClientHT.subscribe(topic);
}
