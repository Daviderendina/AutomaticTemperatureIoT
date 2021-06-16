#ifndef DEVICE_h
#define DEVICE_h

#include <MQTT.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <InfluxDbClient.h>

#include "secrets.h"


char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;


#define TOPIC_DISCOVERY "rt2/discovery"
#define TOPIC_DISCOVERY_RESPONSE "rt2/discovery/response"

#define UPDATE_TIME 1800000


class Device {

  protected:
    bool discoveryStarted = false, discoveryEnded = false;
    bool deviceStatus; // ON/OFF status
    String macAddress = "";
    String deviceID = "";
    MQTTClient *mqttClient;
    WiFiClient networkClient;
    long timeTemp = UPDATE_TIME;

    // Influx connection data
    String INFLUXDB_URL = "";
    String INFLUXDB_TOKEN = "";
    String INFLUXDB_ORG = "";
    String INFLUXDB_BUCKET = "";
    String POINT_DEVICE = "";

    InfluxDBClient *client_idb = NULL;
    Point *pointDevice = NULL;

    void initializeDatabase(JsonObject dbInfo){
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

      client_idb = new InfluxDBClient(
        INFLUXDB_URL.c_str(),
        INFLUXDB_ORG.c_str(),
        INFLUXDB_BUCKET.c_str(),
        INFLUXDB_TOKEN.c_str());

      pointDevice = new Point(POINT_DEVICE);
    }

    void checkAndConnectWifi(){
        if (WiFi.status() != WL_CONNECTED) {
            Serial.print(F("Connecting to SSID: "));
            Serial.println(SECRET_SSID);

            WiFi.begin(ssid, pass);
            while (WiFi.status() != WL_CONNECTED) {
              Serial.print(F("."));
              delay(250);
            }
            Serial.print(F("\nConnected to: "));
            Serial.print(WiFi.SSID());
            Serial.print(F(" - My IP: "));
            Serial.println(WiFi.localIP());
        }

        if(macAddress == "")
          macAddress = WiFi.macAddress() + "-" + deviceID;
    }

    void startMQTTDiscovery(){
        discoveryStarted = true;

        Serial.print(deviceID);
        Serial.println(F(": starting discovery procedure"));

        String description = generateDiscoveryDescriptionMQTT();

        mqttClient->publish(TOPIC_DISCOVERY, description);
    }

    static void newMessageReceivedMQTT(MQTTClient *client, char topic[], char bytes[], int length){
      // dal client prendo il campo ref e mi ritrovo il mio obj

      Device *thisDevice = (Device*) client->ref;
      Serial.print(thisDevice->macAddress);
      Serial.print(F(": Incoming MQTT message: "));
      Serial.print(topic);
      Serial.print(F(" - "));
      Serial.println(bytes);

      String topicStr = String(topic);
      String payload = String(bytes);

      if(strcmp(topic, TOPIC_DISCOVERY_RESPONSE) == 0){
          if(strcmp(bytes, "update_request") == 0 && thisDevice->discoveryEnded)
            thisDevice->handleStatusOnServerReq();
          else if(! thisDevice->discoveryEnded)
            thisDevice->handleDiscoveryResponse(payload);
      }
      else
          thisDevice->handleDeviceMessageReceivedMQTT(topicStr, payload);
    }

    void handleDiscoveryResponse(String payload){
        DynamicJsonDocument response(2048);
        deserializeJson(response, payload);
        const char *mac = response["mac"];

        if(macAddress == mac){
            handleDiscoveryResponseDevice(response);
            discoveryEnded = true;
            Serial.print(deviceID);
            Serial.println(F(": discovery phase finished"));
            Serial.print(deviceID);
            //Serial.print(F(": unsubscribed from topic"));
            //Serial.println(TOPIC_DISCOVERY_RESPONSE);
            //mqttClient->unsubscribe(TOPIC_DISCOVERY_RESPONSE);
            subscribeMQTTTopics();
        }
    }

    void connectToMQTTBroker() {
        if (!this->mqttClient->connected()) {   // not connected
            Serial.print(deviceID);
            Serial.println(F(": Connecting to MQTT broker..."));

            String lastWillMsg = macAddress + " off";
            mqttClient->setWill(TOPIC_DISCOVERY, lastWillMsg.c_str());

            String clientId = MQTT_CLIENTID + deviceID;

            while (!mqttClient->connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
              Serial.print(F("."));
              delay(1000);
            }
            Serial.print(deviceID);
            Serial.println(F(": Connected!"));

            discoveryStarted = false;
            discoveryEnded = false;
            mqttClient->subscribe(TOPIC_DISCOVERY_RESPONSE);
        }
      subscribeMQTTTopics();
    }

    void subscribeSingleTopic(String topic){
      if(topic != "" && topic != NULL && topic != "null")
        mqttClient->subscribe(topic);
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

            if(t1 != t2 && (t1 != "+/" && t2 != "+/")){
              Serial.print(F("No match found between topics "));
              Serial.print(topic1);
              Serial.print(" ");
              Serial.println(topic2);
              return false;
            }
        }
        return topic1 == topic2;
    }

    virtual void subscribeMQTTTopics() = 0;

    virtual void handleDeviceMessageReceivedMQTT(String &topic, String &payload) = 0;

    virtual void handleDiscoveryResponseDevice(JsonDocument &response) = 0;

    virtual String generateDiscoveryDescriptionMQTT() = 0;

    virtual void setupExtra() = 0;
    
    virtual void loopExtra() = 0;

    virtual void handleStatusOnServerReq() = 0;

    



  public:
    Device(char deviceID[], boolean deviceStatus){
        this->deviceID = deviceID;
        this->deviceStatus = deviceStatus;
    }

    void setup(){
        mqttClient = new MQTTClient(2048);
        mqttClient->ref = this;
        mqttClient->begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
        mqttClient->onMessageAdvanced(newMessageReceivedMQTT);

        setupExtra();
    }

    void loop(){

        checkAndConnectWifi();

        connectToMQTTBroker();   // connect to MQTT broker (if not already connected)
        mqttClient->loop();       // MQTT client loop

        // Start discovery if the process is not finished
        if(!discoveryStarted && !discoveryEnded)
            startMQTTDiscovery();

        loopExtra();
    }

    boolean getStatus(){
      return this->deviceStatus;
    }
};


#endif
