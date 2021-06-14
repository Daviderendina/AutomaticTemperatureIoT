#include "device.h"

// Use @MAC@ and MAC address will be added automatically
#define DESCRIPTION "{ 'mac': '@MAC@', 'name': 'Cooling', 'description': 'Sensor description', 'type': 'cooling', 'observes': [ 'temperaturehumidity'], 'threshold': [ { 'id': 'TMED', 'name': 'Temperatura media', 'type': 'Integer', 'value' : '@TMED@' }, { 'id': 'THIGH', 'name': 'Temperatura alta', 'type': 'Integer', 'value' : '@THIGH@' }, { 'id': 'HHIGH', 'name': 'Umidità alta', 'type': 'Integer', 'value' : '@HHIGH@' } ] }"

// Default value for threshold THIGH, which indicate the limit temperature for turning on air conditioned.
#define TEMPERATURE_HIGH_DEFAULT 32

// Default value for threshold TMED and HHIGH, which are used to check if dehumidifier must be turned on
#define TEMPERATURE_MED_DEFAULT 28
#define HUMID_HIGH_DEFAULT 80


class AirConditionedDevice : public Device {

  public:
    AirConditionedDevice() : Device("AC", false){ }

  private:
    String TOPIC_TEMPERATUREHUMIDITY = "";   // Receive temperature and humidity update on this topic
    String TOPIC_THRESHOLD_HUMID = "";       // Receive update on humidity threshold
    String TOPIC_THRESHOLD_TEMPMED = "";     // Receive update on temperature medium threshold
    String TOPIC_THRESHOLD_TEMPHIGH = "";    // Receive update on temperature high threshold
    String TOPIC_STATUS_CHANGE = "";         // Communicate status of this device


    float TEMPERATURE_MED = TEMPERATURE_MED_DEFAULT;
    float TEMPERATURE_HIGH = TEMPERATURE_HIGH_DEFAULT;
    int   HUMID_HIGH = HUMID_HIGH_DEFAULT;


    void loopExtra() {
      int now = millis();
      if(discoveryEnded && now - timeTemp > UPDATE_TIME){
        logStatusChangeInflux();
        timeTemp = millis();
      }
    }

    void setupExtra(){}

    void handleDiscoveryResponseDevice(JsonDocument &response){
      JsonObject thr = response["threshold"];
      String tmed = thr["TMED"];
      String thigh = thr["THIGH"];
      String hhigh = thr["HHIGH"];
      TOPIC_THRESHOLD_HUMID = hhigh;
      TOPIC_THRESHOLD_TEMPMED = tmed;
      TOPIC_THRESHOLD_TEMPHIGH = thigh;

      JsonObject obs = response["observes"];
      String temphumid = obs["temperaturehumidity"];
      TOPIC_TEMPERATUREHUMIDITY = temphumid;

      String statusChange = response["status"];
      TOPIC_STATUS_CHANGE = statusChange;

      JsonObject dbObj = response["database"];
      initializeDatabase(dbObj);

      Serial.println("AC: Listening TMED on: "+TOPIC_THRESHOLD_TEMPMED);
      Serial.println("AC: Listening THIGH on: "+TOPIC_THRESHOLD_TEMPHIGH);
      Serial.println("AC: Listening HHIHG on: "+TOPIC_THRESHOLD_HUMID);
      Serial.println("AC: Listening temperature and humidity on: "+TOPIC_TEMPERATUREHUMIDITY);
      Serial.println("AC: Will communicate status on: "+TOPIC_STATUS_CHANGE);
      
      //subscribeMQTTTopics();
    }

    void logStatusChangeInflux(){
      Serial.println("Ac: logging new status to database instance");

      if(!client_idb->validateConnection()){
        Serial.println("AC: error, cannot reach influxdb instance");
        return;
      }

      pointDevice->clearFields();
      pointDevice->addTag("device", macAddress);
      pointDevice->addField("airconditioned_status", deviceStatus ? 1 : 0);

      Serial.print(F("Writing: "));
      Serial.println(pointDevice->toLineProtocol());
      if (!client_idb->writePoint(*pointDevice)) {
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb->getLastErrorMessage());
      }
    }

    void subscribeMQTTTopics(){
      subscribeSingleTopic(TOPIC_THRESHOLD_TEMPMED);
      subscribeSingleTopic(TOPIC_THRESHOLD_TEMPHIGH);
      subscribeSingleTopic(TOPIC_THRESHOLD_HUMID);
      subscribeSingleTopic(TOPIC_TEMPERATUREHUMIDITY);
    }

    void handleDeviceMessageReceivedMQTT(String &topic, String &payload){
      if(checkTopics(topic, TOPIC_TEMPERATUREHUMIDITY))
        handleNewTemperatureHumidityValue(payload);

      if(checkTopics(topic, TOPIC_THRESHOLD_HUMID))
        handleNewHHighRequest(payload);

      if(checkTopics(topic, TOPIC_THRESHOLD_TEMPMED))
        handleNewTMedRequest(payload);

      if(checkTopics(topic, TOPIC_THRESHOLD_TEMPHIGH))
        handleNewTHighRequest(payload);
    }

    void handleNewHHighRequest(String payload){
      Serial.println("AC: new HHIGH threshold value received "+payload);
      HUMID_HIGH = payload.toFloat();
    }

    void handleNewTMedRequest(String payload){
      Serial.println("AC: new TMED threshold value received "+payload);
      TEMPERATURE_MED = payload.toFloat();
    }

    void handleNewTHighRequest(String payload){
      Serial.println("AC: new THIGH threshold value received "+payload);
      TEMPERATURE_HIGH = payload.toFloat();
    }

    void handleNewTemperatureHumidityValue(String payload){
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

      boolean curStatus = deviceStatus;

      if(temp > TEMPERATURE_HIGH){
        deviceStatus = true;
      } else if (temp > TEMPERATURE_MED && humid > HUMID_HIGH){
        deviceStatus = true;
      } else {
        deviceStatus = false;
      }

      if(curStatus != deviceStatus){
        Serial.print(F("AC: air conditioned status changed, current value: "));
        Serial.println((deviceStatus ? "on" : "off"));
        comunicateStatusChangeMQTT();
        logStatusChangeInflux();
      }
    }

    void handleStatusOnServerReq(){
      String description = generateDiscoveryDescriptionMQTT();
      mqttClient->publish(TOPIC_DISCOVERY, description);

      comunicateStatusChangeMQTT();
    }

    void comunicateStatusChangeMQTT(){
      Serial.print(F("HT: publishing new status on "));
      Serial.println(TOPIC_STATUS_CHANGE);
      mqttClient->publish(TOPIC_STATUS_CHANGE, deviceStatus ? "on" : "off");
    }

    String generateDiscoveryDescriptionMQTT(){
      // Replace threshold value in the JSon description
      String discoveryMessage = DESCRIPTION;
      discoveryMessage.replace("@HHIGH@", String(HUMID_HIGH));
      discoveryMessage.replace("@THIGH@", String(TEMPERATURE_HIGH));
      discoveryMessage.replace("@TMED@", String(TEMPERATURE_MED));
      discoveryMessage.replace("@MAC@", String(macAddress));

      return discoveryMessage;
    }
};
