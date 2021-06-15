// Use @MAC@ and MAC address will be added automatically
#define DESCRIPTION "{ 'mac': '@MAC@', 'name': 'Tilt sensor', 'description': '', 'type': 'sensor', 'sensors' : [{ 'id' : 'tilt', 'name' : 'Tilt sensor', 'measure' : 'tilt', 'unit of measure' : '' }] }"

#include "device.h"

#define TILT D7 


class TiltSensor : public Device {

  public:
    TiltSensor() : Device("TL", true){}

  private:
    String TOPIC_TILT_UPDATE = "";   // Update tilt value on this topic
    String TOPIC_TILT_STATUS = "";   // Listen tilt status change (on/off tilt sensors)
    boolean tiltStatus = true;
    byte tiltValue;
    long timeTemp = 0;


    void loopExtra() {
      if(millis() - timeTemp > 1000){
        byte newTiltValue = digitalRead(TILT);
        if(newTiltValue != tiltValue){
          Serial.println("TL: Found different tilt status: " + String(newTiltValue));
          tiltValue = newTiltValue;
          comunicateNewTiltMQTT(tiltValue);
        }
        timeTemp = millis();
      }
    }

    void setupExtra(){
      pinMode(TILT, INPUT_PULLUP);
      tiltValue = digitalRead(TILT);
    }

    void comunicateNewTiltMQTT(byte tiltValue){
      Serial.print(F("TL: comunicating tilt value change on MQTT topic "));
      Serial.println(TOPIC_TILT_UPDATE);
      mqttClient->publish(TOPIC_TILT_UPDATE, tiltValue == 1 ? "open" : "close");
    }

    void handleDiscoveryResponseDevice(JsonDocument &response){
      JsonObject obs = response["sensors"];
      JsonObject tilt = obs["tilt"];
      String tiltUpdate = tilt["update"];
      String tiltStatus = tilt["status"];
      TOPIC_TILT_UPDATE = tiltUpdate;
      TOPIC_TILT_STATUS = tiltStatus;
  
      Serial.println("TL: Listening status on: " + TOPIC_TILT_STATUS);
      Serial.println("TL: Will communicate tilt on: " + TOPIC_TILT_UPDATE);

      subscribeMQTTTopics();

      JsonObject dbObj = response["database"];
      initializeDatabase(dbObj);

    }

    void logStatusChangeInflux(){
      Serial.println("TL: logging new status to database instance");

      if(!client_idb->validateConnection()){
        Serial.println("TL: error, cannot reach influxdb instance");
        return;
      }

      pointDevice->clearFields();
      pointDevice->addTag("device", macAddress);
      pointDevice->addField("tilt", tiltValue);
  
      Serial.print(F("Writing: "));
      Serial.println(pointDevice->toLineProtocol());
      if (!client_idb->writePoint(*pointDevice)) {
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb->getLastErrorMessage());
      }
    }

    void subscribeMQTTTopics(){
      subscribeSingleTopic(TOPIC_TILT_STATUS);
    }

    void handleDeviceMessageReceivedMQTT(String &topic, String &payload){
      if(checkTopics(topic, TOPIC_TILT_STATUS))
        handleStatusChangeReq(payload);
    }

    void handleStatusChangeReq(String payload){
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

    String generateDiscoveryDescriptionMQTT(){
      String discoveryMessage = DESCRIPTION;
      discoveryMessage.replace("@MAC@", macAddress);

      return discoveryMessage;
    }

    void handleStatusOnServerReq(){
      String description = generateDiscoveryDescriptionMQTT();
      mqttClient->publish(TOPIC_DISCOVERY, description);
      mqttClient->publish(TOPIC_TILT_STATUS, tiltStatus ? "on" : "off");

    }
};
