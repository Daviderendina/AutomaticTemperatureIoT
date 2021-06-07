#include "description.h"

#define TOPIC_DISCOVERY "rt/discovery"
#define TOPIC_DISCOVERY_RESPONSE "rt/discovery/response"

String TOPIC_TILT_UPDATE = "";
String TOPIC_TILT_STATUS = "";

String TOPIC_TEMPERATURE_UPDATE = "";
String TOPIC_TEMPERATURE_STATUS = "";

String TOPIC_HUMIDITY_UPDATE = "";
String TOPIC_HUMIDITY_STATUS = "";

String TOPIC_TEMPERATURE_HUMIDITY_UPDATE = "";

String TOPIC_DEVICE_STATUS_CHANGE = "";

String TOPIC_WINDOW_ALERT = "";


void publishTiltMQTT(){
  mqttClient.publish(TOPIC_TILT_UPDATE, tiltValue == 1 ? "open" : "close");
}

void performDiscovery(){
  Serial.println("Starting discovery procedure");
  discoveryStarted = true;

  String description = DESCRIPTION;
  description.replace("@MAC@", macAddr);

  mqttClient.publish(TOPIC_DISCOVERY, description);
}

void handleDiscoveryResponse(String payload){

  DynamicJsonDocument response(2048);
  deserializeJson(response, payload);
  const char *mac = response["mac"];

  if(macAddr == mac){

    // Retrieve measures topic
    JsonObject obs = response["observes"];
    String tilt = obs["tilt"];
    TOPIC_TILT_UPDATE = tilt;

    String devStatus = obs["deviceStatus"];
    TOPIC_DEVICE_STATUS_CHANGE = devStatus;

    // Retrieve sensors topic
    JsonObject sens = response["sensors"];
    
    JsonObject tiltSens = sens["tilt"];
    String tiltUpdate = tiltSens["update"];
    String tiltStatus = tiltSens["status"];
    TOPIC_TILT_UPDATE = tiltUpdate;
    TOPIC_TILT_STATUS = tiltStatus;

    JsonObject tempSens = sens["temperature"];
    String tempUpdate = tempSens["update"];
    String tempStatus = tempSens["status"];
    TOPIC_TEMPERATURE_UPDATE = tempUpdate;
    TOPIC_TEMPERATURE_STATUS = tempStatus;

    JsonObject humidSens = sens["humidity"];
    String humidUpdate = humidSens["update"];
    String humidStatus = humidSens["status"];
    TOPIC_HUMIDITY_UPDATE = humidUpdate;
    TOPIC_HUMIDITY_STATUS = humidStatus;

    // Retrieve alert topic
    JsonObject alertObj = response["alert"];
    String alertTopic = alertObj["windowOpen"];
    TOPIC_WINDOW_ALERT = alertTopic;

    // Retrieve special temperaturehumidity topic
    String temperatureHumidity = response["temperaturehumidity"];
    TOPIC_TEMPERATURE_HUMIDITY_UPDATE = temperatureHumidity;

    subscribeTopicsMQTT();

    Serial.println("Listening tilt value update on: "+TOPIC_TILT_UPDATE);
    Serial.println("Will communicate temperature on: "+TOPIC_TEMPERATURE_UPDATE);
    Serial.println("Will communicate humidity on: "+TOPIC_HUMIDITY_UPDATE);
    Serial.println("Will communicate temperaturehumidity on: "+TOPIC_TEMPERATURE_HUMIDITY_UPDATE);
    Serial.println("Will communicate tilt on: "+TOPIC_TILT_UPDATE);
    Serial.println("Will communicate window alert on: "+TOPIC_WINDOW_ALERT);
    Serial.println("Listening temperature sensor status change on: " + TOPIC_TEMPERATURE_STATUS);
    Serial.println("Listening humidity sensor status change on: " + TOPIC_HUMIDITY_STATUS);
    Serial.println("Listening tilt sensor status change on: " + TOPIC_TILT_STATUS);
    Serial.println("Listening device status change on: "+TOPIC_DEVICE_STATUS_CHANGE);
    


    JsonObject dbInfo = response["database"];
    String influxUrl = dbInfo["influxdb_url"];
    String influxToken = dbInfo["influxdb_token"];
    String influxOrg = dbInfo["influxdb_org"];
    String infuxBucket = dbInfo["influxdb_bucket"];
    String influxPointDeviceName = dbInfo["influxdb_pointDevice"];

    INFLUXDB_URL = influxUrl;
    INFLUXDB_TOKEN = influxToken;
    INFLUXDB_ORG = influxOrg;
    INFLUXDB_BUCKET = infuxBucket;
    POINT_DEVICE = influxPointDeviceName;
    
    discoveryEnded = true;
    Serial.println(F("Discovery phase finished"));
  }
}

void mqttMessageReceived(String &topic, String &payload) {
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);

  if(checkTopics(topic, TOPIC_DISCOVERY_RESPONSE) && !discoveryEnded)
    handleDiscoveryResponse(payload);

  if(checkTopics(topic, TOPIC_TEMPERATURE_STATUS))
    handleSensorStatusChangeReq(payload, tempStatus, "temperature sensor", temperatureMeasures, true);
  
  if(checkTopics(topic, TOPIC_HUMIDITY_STATUS))
    handleSensorStatusChangeReq(payload, humidStatus, "humidity sensor", humidityMeasures, true);
    
  if(checkTopics(topic, TOPIC_TILT_STATUS))
    handleSensorStatusChangeReq(payload, tiltStatus, "window sensor", NULL, false);  

  if(checkTopics(topic, TOPIC_DEVICE_STATUS_CHANGE))
    handleDeviceStatusChange(payload);

  if(checkTopics(topic, TOPIC_TILT_UPDATE))
    handleDeviceTiltUpdate(payload);

}

void mqttSetup(){
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);              // callback on message received from MQTT broker
}

void connectToMQTTBrokerAndSubscribe() {
  if (!mqttClient.connected()) {   // not connected
    Serial.print(F("\nConnecting to MQTT broker..."));
    String lastWillMsg = macAddr + " off";
    mqttClient.setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));

    discoveryStarted = false;
    discoveryEnded = false;

    subscribeTopicsMQTT();
  }
}

void subscribeTopicsMQTT(){
  subscribeSingleTopic(TOPIC_DISCOVERY_RESPONSE);
  subscribeSingleTopic(TOPIC_TEMPERATURE_STATUS);
  subscribeSingleTopic(TOPIC_HUMIDITY_STATUS);
  subscribeSingleTopic(TOPIC_TILT_STATUS);
  subscribeSingleTopic(TOPIC_TILT_UPDATE);
  subscribeSingleTopic(TOPIC_DEVICE_STATUS_CHANGE);
  
  Serial.println(F("Topics subscribe activity completed"));
}

void handleSensorStatusChangeReq(String payload, boolean& statusFlag, String logSensorName, MeasureArrayHandler arrayMeasures, bool arrayMeasuresPresent){
  if (payload == "on") {
    statusFlag = true;
    instantUpdate = true;
    Serial.println("Request received: "+ logSensorName +" ON");
  } else if (payload == "off") {
    if(arrayMeasuresPresent)
      arrayMeasures.clearMeasures();
    statusFlag = false;
    instantUpdate = true;
    Serial.println("Request received: "+ logSensorName +" OFF");
  } else {
    Serial.println(F("MQTT Payload not recognized, message skipped"));
  }
}

void communicateValuesMQTT(float temperature, float humidity){
  Serial.println("Publishing temperature, humidity on MQTT");
  if(tempStatus)
    mqttClient.publish(TOPIC_TEMPERATURE_UPDATE, String(temperature));

  if(humidStatus)
    mqttClient.publish(TOPIC_HUMIDITY_UPDATE, String(humidity));

  if(humidStatus || tempStatus){
    String temperatureHumidityMsg = (tempStatus ? String(temperature) : "off") + " " + (humidStatus ? String(humidity) : "off");
    mqttClient.publish(TOPIC_TEMPERATURE_HUMIDITY_UPDATE, temperatureHumidityMsg); 
  }
}

void communicateAlertMQTT(){
  mqttClient.publish(TOPIC_WINDOW_ALERT, windowAlert? "on" : "off");
}

void subscribeSingleTopic(String topic){
  if(topic != "" && topic != NULL && topic != "null")
    mqttClient.subscribe(topic);
}

bool checkTopics(String topic1, String topic2){
    int posEnd1 = topic1.indexOf('/') + 1;
    int posEnd2 = topic2.indexOf('/') + 1;
    
    int n1 = std::count(topic1.begin(), topic1.end(), '/');
    int n2 = std::count(topic2.begin(), topic2.end(), '/');
    
    if(n1 != n2) return false;
    
    for(int i = 0; i < n1; i++){
        
        String t1 = topic1.substring(0, posEnd1);
        String t2 = topic2.substring(0, posEnd2);
        
        topic1.remove(0, posEnd1);
        topic2.remove(0, posEnd2);
        
        posEnd1 = topic1.indexOf('/') + 1;
        posEnd2 = topic2.indexOf('/') + 1;
        
        if(t1 != t2 && (t1 != "+/" && t2 != "+/"))
            return false;
    }
    return topic1 == topic2;
}
