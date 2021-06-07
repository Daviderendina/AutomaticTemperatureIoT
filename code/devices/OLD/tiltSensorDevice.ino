#include "tiltDescription.h"
 
#define TILT D5 

// MQTT Topics for communicating with network
String TL_TOPIC_TILT_UPDATE = "";   // Update tilt value on this topic
String TL_TOPIC_TILT_STATUS = "";   // Listen tilt status change (on/off tilt sensors)

// InfluxDB data
String TL_INFLUXDB_URL = "";
String TL_INFLUXDB_TOKEN = "";
String TL_INFLUXDB_ORG = "";
String TL_INFLUXDB_BUCKET = "";
String TL_POINT_DEVICE = "";

// Flags used for checking discovery protocol status
boolean discoveryStartedTL = false;
boolean discoveryEndedTL = false;

String macAddressTL = "";

MQTTClient mqttClientTL(2048);
WiFiClient networkClientTL;

boolean tiltStatus = true;
byte tiltValue;


void setupTiltSensor(){
  pinMode(TILT, INPUT_PULLUP);
  tiltValue = digitalRead(TILT);
  
  mqttClientTL.begin(MQTT_BROKERIP, 1883, networkClientTL);   // setup communication with MQTT broker
  mqttClientTL.onMessage(mqttMessageReceivedTL);              // callback on message received from MQTT broker

  macAddressTL = WiFi.macAddress() + "-TL";
}

void loopTiltSensor(){
  checkAndConnectWifi();

  connectToMQTTBrokerTL();   
  mqttClientTL.loop();       
  
  // Start discovery if the process is not finished
  if(!discoveryStartedTL && !discoveryEndedTL)
    startMQTTDiscoveryTL();

  // Check tilt value
  if(tiltStatus && discoveryEndedTL){
    byte newTiltValue = digitalRead(TILT);
    if(newTiltValue != tiltValue){
      Serial.println("TL: Found different tilt status: " + String(newTiltValue));
      tiltValue = newTiltValue;
      comunicateNewTiltMQTTTL(tiltValue);
    }
  }
}

void comunicateNewTiltMQTTTL(byte tiltValue){
  Serial.print(F("TL: comunicating tilt value change on MQTT topic"));
  Serial.println(TL_TOPIC_TILT_UPDATE);
  mqttClientTL.publish(TL_TOPIC_TILT_UPDATE, tiltValue == 1 ? "open" : "close");
}

void startMQTTDiscoveryTL(){
  Serial.println("TL: starting discovery procedure");
  discoveryStartedTL = true;

  // Replace threshold value in the JSon description
  String description = TL_DESCRIPTIon;
  description.replace("@MAC@", macAddressTL);
  
  mqttClientTL.publish(TOPIC_DISCOVERY, description);
}

void mqttMessageReceivedTL(String &topic, String &payload) {
  Serial.println("TL: Incoming MQTT message: " + topic + " - " + payload);

  if(checkTopics(topic, TOPIC_DISCOVERY_RESPONSE) && !discoveryEndedTL)
    handleDiscoveryResponseTL(payload);

  if(checkTopics(topic, TL_TOPIC_TILT_STATUS))
    handleStatusChangeReqTL(payload);
}

void handleStatusChangeReqTL(String payload){
  if (payload == "on") {
    tiltStatus = true;
    Serial.println(F("TL: Request received: status on"));
  } else if (payload == "off") {
    tiltStatus = false;
    Serial.println(F("TL: Request received: status OFF"));
  } else {
    Serial.println(F("TL: MQTT Payload not recognized, message skipped"));
  }
}

void handleDiscoveryResponseTL(String payload){

  DynamicJsonDocument response(2048);
  deserializeJson(response, payload);
  const char *mac = response["mac"];

  if(macAddressTL == mac){
    discoveryEndedTL = true;
    
    JsonObject obs = response["sensors"];
    JsonObject tilt = obs["tilt"];
    String tiltUpdate = tilt["update"];
    String tiltStatus = tilt["status"];
    TL_TOPIC_TILT_UPDATE = tiltUpdate;
    TL_TOPIC_TILT_STATUS = tiltStatus;

    Serial.println("TL: Listening status on: " + TL_TOPIC_TILT_STATUS);
    Serial.println("TL: Will communicate tilt on: " + TL_TOPIC_TILT_UPDATE);
    subscribeSingleTopicTL(TL_TOPIC_TILT_STATUS);

    JsonObject dbInfo = response["database"];
    String influxUrl = dbInfo["influxdb_url"];
    String influxToken = dbInfo["influxdb_token"];
    String influxOrg = dbInfo["influxdb_org"];
    String infuxBucket = dbInfo["influxdb_bucket"];
    String influxPointDeviceName = dbInfo["influxdb_pointDevice"];
    
    TL_INFLUXDB_URL = influxUrl;
    TL_INFLUXDB_TOKEN = influxToken;
    TL_INFLUXDB_ORG = influxOrg;
    TL_INFLUXDB_BUCKET = infuxBucket;
    TL_POINT_DEVICE = influxPointDeviceName;
    
    Serial.println(F("TL: discovery phase finished"));
  }
}

void logStatusChangeInfluxTL(){ 
  Serial.println("TL: logging tilt value to database instance");
  
  InfluxDBClient client_idb(
    TL_INFLUXDB_URL.c_str(), 
    TL_INFLUXDB_ORG.c_str(), 
    TL_INFLUXDB_BUCKET.c_str(), 
    TL_INFLUXDB_TOKEN.c_str());
  Point pointDevice(TL_POINT_DEVICE);

  if(!client_idb.validateConnection()){
    Serial.println("TL: error, cannot reach influxdb instance");
    return;
  }

  pointDevice.addTag("device", macAddressTL);
  pointDevice.addField("tilt", tiltValue);
  
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void connectToMQTTBrokerTL() {
  if (!mqttClientTL.connected()) {   // not connected
    Serial.print(F("\nTL: Connecting to MQTT broker..."));
    
    String lastWillMsg = macAddressTL + " off";
    mqttClientTL.setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());
    
    while (!mqttClientTL.connect(TL_MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nTL: Connected!"));

    discoveryStartedTL = false;
    discoveryEndedTL = false;
  }

  mqttClientTL.subscribe(TOPIC_DISCOVERY_RESPONSE);
  subscribeMQTTTopicsTL();
}

void subscribeMQTTTopicsTL(){
  subscribeSingleTopicTL(TL_TOPIC_TILT_STATUS);
}

void subscribeSingleTopicTL(String topic){
  if(topic != "" && topic != NULL && topic != "null")
    mqttClientTL.subscribe(topic);
}
