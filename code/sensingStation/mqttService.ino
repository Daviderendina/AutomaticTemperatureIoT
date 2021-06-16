#include "description.h"

#define TOPIC_DISCOVERY "rt2/discovery"
#define TOPIC_DISCOVERY_RESPONSE "rt2/discovery/response"

String TOPIC_TEMPERATURE_UPDATE = "";
String TOPIC_TEMPERATURE_STATUS = "";

String TOPIC_HUMIDITY_UPDATE = "";
String TOPIC_HUMIDITY_STATUS = "";

String TOPIC_TEMPERATURE_HUMIDITY_UPDATE = "";


void performDiscovery(){
  Serial.println("Starting discovery procedure");

  String description = DESCRIPTION;
  description.replace("@MAC@", macAddr);
  
  description.replace("@TEMPSTATUS@", tempStatus ? "on" : "off");
  description.replace("@HUMIDSTATUS@", humidStatus ? "on" : "off");
  int lastValue = temperatureMeasures.getAverage();
  description.replace("@TEMPVALUE@", String(lastValue));
  lastValue = humidityMeasures.getAverage();
  description.replace("@HUMIDVALUE@", String(lastValue));

  mqttClient.subscribe(TOPIC_DISCOVERY_RESPONSE);
  mqttClient.publish(TOPIC_DISCOVERY, description);
}

void handleDiscoveryResponse(String payload){

  DynamicJsonDocument response(2048);
  deserializeJson(response, payload);
  const char *mac = response["mac"];

  if(macAddr == mac){
    
    discoveryEnded = true;

    // Retrieve sensors topic
    JsonObject sens = response["sensors"];
    
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


    // Retrieve special temperaturehumidity topic
    String temperatureHumidity = response["temperaturehumidity"];
    TOPIC_TEMPERATURE_HUMIDITY_UPDATE = temperatureHumidity;

    //subscribeTopicsMQTT();

    Serial.println("Will communicate temperature on: "+TOPIC_TEMPERATURE_UPDATE);
    Serial.println("Will communicate humidity on: "+TOPIC_HUMIDITY_UPDATE);
    Serial.println("Will communicate temperaturehumidity on: "+TOPIC_TEMPERATURE_HUMIDITY_UPDATE);
    Serial.println("Listening temperature sensor status change on: " + TOPIC_TEMPERATURE_STATUS);
    Serial.println("Listening humidity sensor status change on: " + TOPIC_HUMIDITY_STATUS);
    

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
    
    Serial.println(F("Discovery phase finished"));
  }
}

void communicateSensorStatus(){
  mqttClient.unsubscribe(TOPIC_TEMPERATURE_STATUS);
  mqttClient.publish(TOPIC_TEMPERATURE_STATUS, tempStatus ? "on" : "off");
  mqttClient.unsubscribe(TOPIC_HUMIDITY_STATUS);
  mqttClient.publish(TOPIC_HUMIDITY_STATUS, humidStatus ? "on" : "off");
}

void mqttMessageReceived(String &topic, String &payload) {
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);

  if(checkTopics(topic, TOPIC_DISCOVERY_RESPONSE) && !discoveryEnded)
    handleDiscoveryResponse(payload);

  if(checkTopics(topic, TOPIC_TEMPERATURE_STATUS))
    handleSensorStatusChangeReq(payload, tempStatus, "temperature sensor", temperatureMeasures, true, temperatureStatusReceived);
  
  if(checkTopics(topic, TOPIC_HUMIDITY_STATUS))
    handleSensorStatusChangeReq(payload, humidStatus, "humidity sensor", humidityMeasures, true, humidityStatusReceived);
}

void mqttSetup(){
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);              // callback on message received from MQTT broker
}

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {   // not connected
    Serial.print(F("\nConnecting to MQTT broker..."));
    String lastWillMsg = macAddr + " off";
    mqttClient.setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));

    discoveryEnded = false;
  }
}

void subscribeTopicsMQTT(){
  Serial.println(F("Topics subscribe activity completed"));
  subscribeSingleTopic(TOPIC_DISCOVERY_RESPONSE, 0);
  subscribeSingleTopic(TOPIC_TEMPERATURE_STATUS, 1);
  subscribeSingleTopic(TOPIC_HUMIDITY_STATUS, 1);
}

void handleSensorStatusChangeReq(String payload, boolean& statusFlag, String logSensorName, MeasureArrayHandler arrayMeasures, bool arrayMeasuresPresent, boolean& flag){
  if (payload == "on") {
    statusFlag = true;
    Serial.println("Request received: "+ logSensorName +" ON");
  } else if (payload == "off") {
    if(arrayMeasuresPresent)
      arrayMeasures.clearMeasures();
    statusFlag = false;
    Serial.println("Request received: "+ logSensorName +" OFF");
  } else {
    Serial.println(F("MQTT Payload not recognized, message skipped"));
    return;
  }

  flag = true;
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


void subscribeSingleTopic(String topic, int QoS){
  if(topic != "" && topic != NULL && topic != "null")
    mqttClient.subscribe(topic, QoS);
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
