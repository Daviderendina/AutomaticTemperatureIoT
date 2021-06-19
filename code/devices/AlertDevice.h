// Use @MAC@ and MAC address will be added automatically
#define DESCRIPTION "{ 'mac': '@MAC@', 'name': 'Alert device', 'description': '', 'type': 'alert', 'observes': ['alert'] }"

#include "device.h"

#define LED D5 

class AlertDevice : public Device {

  public:
    AlertDevice() : Device("AL", true){}

  private:
    String TOPIC_ALERT = "";   // Update tilt value on this topic

    void loopExtra() {}

    void setupExtra(){
      pinMode(D5, OUTPUT);
      digitalWrite(D5, HIGH);
    }
    
    void handleDiscoveryResponseDevice(JsonDocument &response){
      JsonObject obs = response["observes"];
      String alert = obs["alert"];
      TOPIC_ALERT = alert;

      Serial.println("AL: Listening alert on: " + TOPIC_ALERT);

      subscribeMQTTTopics();
    }

    void subscribeMQTTTopics(){
      subscribeSingleTopic(TOPIC_ALERT);
    }

    void handleDeviceMessageReceivedMQTT(String &topic, String &payload){
      if(checkTopics(topic, TOPIC_ALERT))
        handleAlertStatusChange(payload);
    }

    void handleAlertStatusChange(String payload){
      if (payload == "on") {
        digitalWrite(D5, LOW);
      } else if (payload == "off") {
        digitalWrite(D5, HIGH);
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
    }
};
