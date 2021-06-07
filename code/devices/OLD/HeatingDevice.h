// Use @MAC@ and MAC address will be added automatically
// Use @TON@ string in SENSOR_DESCRIPTIon in threshold Ton value field
// Use @TOFF@ string in SENSOR_DESCRIPTIon in threshold TOFF value field
#define DESCRIPTION "{ 'mac': '@MAC@', 'name': 'Heating', 'description': 'Sensor description', 'type': 'heating', 'observes': [ 'temperature' ], 'threshold': [ { 'id': 'TOFF', 'name': 'Temperatura spegnimento', 'type': 'Integer', 'value' : '@TOFF@' }, { 'id': 'Ton', 'name': 'Temperatura accensione', 'type': 'Integer', 'value' : '@TON@' } ] }"

#define TON_THRESHOLD_DEFAULT 19
#define TOFF_THRESHOLD_DEFAULT 21

#include "device.h"

class HeatingDevice : Device {

  public:
    HeatingDevice() : Device("HT"){ }

  private:
    String TOPIC_TEMPERATURE = "";     // Receive update on temperature value
    String TOPIC_THRESHOLD_TON = "";   // Receive update on temperature_on threshold (turn on heating)
    String TOPIC_THRESHOLD_TOFF = "";  // Receive update on temperature_off threshold (turn off heating)
    String TOPIC_STATUS_CHANGE = "";   // Communicate device status change

    float TEMPERATURE_OFF = TOFF_THRESHOLD_DEFAULT;
    float TEMPERATURE_ON = TON_THRESHOLD_DEFAULT;


    void updateStatusLoop() {
      int now = millis();
      if(discoveryEnded && now - timeTemp > UPDATE_TIME){
        logStatusChangeInflux();
        timeTemp = millis();
      }
    }

    void handleDiscoveryResponseDevice(String &response){
      // Read received topics
      JsonObject thr = response["threshold"];
      String ton = thr["TON"];
      String toff = thr["TOFF"];
      TOPIC_THRESHOLD_TON = ton;
      TOPIC_THRESHOLD_TOFF = toff;

      JsonObject obs = response["observes"];
      String temp = obs["temperature"];
      TOPIC_TEMPERATURE = temp;

      String statusChange = response["status"];
      TOPIC_STATUS_CHANGE = statusChange;

      Serial.println("HT: Listening TON on: " + TOPIC_THRESHOLD_TON);
      Serial.println("HT: Listening TOFF on: " + TOPIC_THRESHOLD_TOFF);
      Serial.println("HT: Listening temperature on: " + TOPIC_TEMPERATURE);
      Serial.println("HT: Will communicate status on: " + TOPIC_STATUS_CHANGE);
      subscribeSingleTopic(TOPIC_TEMPERATURE);
      subscribeSingleTopic(TOPIC_THRESHOLD_TON);
      subscribeSingleTopic(TOPIC_THRESHOLD_TOFF);
    }

    void logStatusChangeInflux(){
      Serial.println("HT: logging new status to database instance");

      if(!client_idb.validateConnection()){
        Serial.println("HT: error, cannot reach influxdb instance");
        return;
      }

      pointDevice.addTag("device", macAddress);
      pointDevice.addField("heating_status", deviceStatus ? 1 : 0);

      Serial.print(F("Writing: "));
      Serial.println(pointDevice.toLineProtocol());
      if (!client_idb.writePoint(pointDevice)) {
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb.getLastErrorMessage());
      }
    }

    void subscribeMQTTTopics(){
      subscribeSingleTopic(TOPIC_DISCOVERY_RESPONSE);
      subscribeSingleTopic(TOPIC_TEMPERATURE);
      subscribeSingleTopic(TOPIC_THRESHOLD_TON);
      subscribeSingleTopic(TOPIC_THRESHOLD_TOFF);
    }

    void handleDeviceMessageReceivedMQTT(String &topic, String &payload){
      if(checkTopics(topic, TOPIC_TEMPERATURE))
        handleNewTemperatureValueHT(payload);

      if(checkTopics(topic, TOPIC_THRESHOLD_TON))
        handleNewTOnRequestHT(payload);

      if(checkTopics(topic, TOPIC_THRESHOLD_TOFF))
        handleNewTOffRequestHT(payload);
    }

    void handleNewTemperatureValueHT(String payload){
      Serial.print(F("HT: new temperature value received "));
      Serial.println(payload);
      float curTemp = payload.toFloat();

      // Calculate current heating status, according to temperature received
      boolean curStatus = deviceStatus;
      if(curTemp < TEMPERATURE_ON && ! deviceStatus){
        curStatus = true;
      } else if (curTemp > TEMPERATURE_OFF && deviceStatus){
        curStatus = false;
      }

      // If the status change, perform status change operations
      if(curStatus != deviceStatus){
        deviceStatus = curStatus;
        Serial.print(F("HT: heating status changed, current value: "));
        Serial.println((deviceStatus ? "on" : "off"));
        comunicateStatusChangeMQTT();
        logStatusChangeInflux();
      }
    }

    void comunicateStatusChangeMQTT(){
      Serial.print(F("HT: publishing new status on "));
      Serial.println(TOPIC_STATUS_CHANGE);
      mqttClientHT.publish(TOPIC_STATUS_CHANGE, deviceStatus ? "on" : "off");
    }

    void handleNewTOnRequestHT(String payload){
      Serial.print(F("HT: new TON threshold value received "));
      Serial.println(payload);
      TEMPERATURE_ON = payload.toFloat();
    }

    void handleNewTOffRequestHT(String payload){
      Serial.print(F("HT: new TOFF threshold value received "));
      Serial.println(payload);
      TEMPERATURE_OFF = payload.toFloat();
    }

    String generateDiscoveryDescriptionMQTT(){
      // Replace threshold value in the JSon description
      String discoveryMessage = DESCRIPTION;
      discoveryMessage.replace("@TON@", String(TEMPERATURE_ON));
      discoveryMessage.replace("@TOFF@", String(TEMPERATURE_OFF));
      discoveryMessage.replace("@MAC@", macAddressHT);

      return discoveryMessage;
    }


}
