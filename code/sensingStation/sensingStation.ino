#include "secrets.h"            

#include <LiquidCrystal_I2C.h>  // display library
#include <Wire.h>               // I2C library
#include <DHT.h>
#include <InfluxDbClient.h> // InfluxDB library
#include <MQTT.h>
#include <ESP8266WiFi.h>        // Wifi
#include <ArduinoJson.h>
#include <EEPROM.h>

#include "MeasureArrayHandler.h"

// Pin mapping
#define DHTPIN D3   
#define TILT D5       
#define CRIT_LED D7  

// Costant for hardware
#define DHTTYPE DHT11   // sensor type DHT 11
#define BUTTON_DEBOUNCE_DELAY 20  // Button debounce

// Software constants
#define SIZE_MEASURES_AVG_ARRAY    3         // Store last 3 values measured
#define MEMORY_DEBUG 0


// Initialize objects for sensors
DHT dht = DHT(DHTPIN, DHTTYPE);

// MQTT Communication
MQTTClient mqttClient(2048);                    
WiFiClient networkClient;

// Discovery flags
boolean discoveryEnded = false;
boolean humidityStatusReceived, temperatureStatusReceived;

// InfluxDB parameters
String INFLUXDB_URL = "";
String INFLUXDB_TOKEN = "";
String INFLUXDB_ORG = "";
String INFLUXDB_BUCKET = "";
String POINT_DEVICE = "";

// WiFi configuration
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Sensor status (ON/OFF) flags 
boolean tempStatus = true;
boolean humidStatus = true;

// Global variables for values
MeasureArrayHandler temperatureMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler humidityMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler rssiMeasures(SIZE_MEASURES_AVG_ARRAY);
float heatIndex;
String macAddr;
int loopCounter;

// EEPROM variables
short eeprom_init;
int single_array_measure_size, eprom_size;
int temp_addr, humid_addr, rssi_addr;



void setup() {
  // Faccio il setup
  initializeHardware();

  while (!Serial) { }

  initializeEEPROM();

  connectWifi();
  macAddr = WiFi.macAddress();

  connectToMQTTBroker();

  performDiscovery();
  while(!discoveryEnded){mqttClient.loop(); delay(5);}
  Serial.println("Finished discovery procedure");

  humidityStatusReceived = false;
  temperatureStatusReceived = false;
  subscribeTopicsMQTT();

  // Aspetto di ricevere notizie
  waitForSensorStatusUpdateMQTT();

  // Communicate status of its sensor on the network
  communicateSensorStatus();

  Serial.print("Temperature: ");
  temperatureMeasures.printArray();
  Serial.print("Humidity: ");
  humidityMeasures.printArray();
  Serial.print("RSSI: ");
  rssiMeasures.printArray();

  readValuesFromEEPROM();  
  
  readSensors();
  
  writeValuesOnEEPROM();

  handleNetworkUpdate();
  
  Serial.println("Going deep sleep for 10 minutes");
  ESP.deepSleep(600e6);
}


void loop() {}

// Hardware

void initializeHardware(){
  Serial.begin(115200);

  mqttSetup();
  WiFi.mode(WIFI_STA);
  
  dht.begin();
  
  pinMode(CRIT_LED, OUTPUT);
  digitalWrite(CRIT_LED, HIGH);
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void connectWifi(){  
  digitalWrite(CRIT_LED, LOW);
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
  digitalWrite(CRIT_LED, HIGH);
}


// Sensors read

void readSensors(){
  if(tempStatus)
    readTemperatureDHT();

  if(humidStatus)
    readHumidityDHT();

  int rssiValue= WiFi.RSSI();
  rssiMeasures.addElement(rssiValue);

  if(MEMORY_DEBUG == 1){
    Serial.print("Temperature: ");
    temperatureMeasures.printArray();
    Serial.print("Humidity: ");
    humidityMeasures.printArray();
    Serial.print("RSSI: ");
    rssiMeasures.printArray();
  }
}

void readTemperatureDHT(){
  delay(2000);
  float tempValue = dht.readTemperature();
  Serial.println("Temp: " + String(tempValue) +"C");
  if(! is.nan(tempValue))
    temperatureMeasures.addElement(tempValue);
}

void readHumidityDHT(){
  delay(2000);
  float humidValue = dht.readHumidity();
  Serial.println("Humidity: " + String(humidValue) +"%");
  if(! is.nan(humidValue))
    humidityMeasures.addElement(humidValue);
}

void calculateHeatIndex(){
  heatIndex = dht.computeHeatIndex(temperatureMeasures.getAverage(), humidityMeasures.getAverage(), false);
  Serial.println("PercTemp: " + String(heatIndex) +"C");
}


// Other

void communicateValues(){
  if(humidStatus && tempStatus)
      calculateHeatIndex();

    // Write temperature and humidity on MQTT
    communicateValuesMQTT(temperatureMeasures.getAverage(), humidityMeasures.getAverage());

    // Update influxDB instance
    writeOnDB();
}

void waitForSensorStatusUpdateMQTT(){
  Serial.println("Waiting for sensors update");
  unsigned long timeTemp = millis();
  unsigned long elapsedTime = 0;
  while(!(temperatureStatusReceived && humidityStatusReceived)){
    mqttClient.loop(); 
    delay(5);
    elapsedTime = millis() - timeTemp; 
    delay(5);
    if(elapsedTime > 5000)
      break;
  }
}

void handleNetworkUpdate(){
  Serial.println(F("Actual loop counter value: "));
  Serial.println(loopCounter);
  if(loopCounter == 3)
    communicateValues();

  EEPROM.write(4, loopCounter % 3);
  EEPROM.commit();
  delay(300);
}


// EEPROM 

void initializeEEPROM(){
  eeprom_init = 0;
  single_array_measure_size = sizeof(float) * (3 + SIZE_MEASURES_AVG_ARRAY);
  eprom_size = 3 * single_array_measure_size + sizeof(loopCounter) + sizeof(eeprom_init);

  temp_addr = sizeof(loopCounter) + sizeof(eeprom_init);
  humid_addr = temp_addr + single_array_measure_size;
  rssi_addr = humid_addr + single_array_measure_size;
  
  Serial.print(F("Initializing EEPROM with size: "));
  Serial.println(eprom_size);
  EEPROM.begin(eprom_size);
  delay(100);
}

void readValuesFromEEPROM(){
  eeprom_init = EEPROM.read(0);
  Serial.print(F("Reading from EEPROM, init value: "));
  Serial.println(eeprom_init);
  if(eeprom_init != 1){
    Serial.println("Nothing has been initialized yet");
    loopCounter = 0;
  } else {
    temperatureMeasures = readMeasureArray(temp_addr);
    humidityMeasures = readMeasureArray(humid_addr);
    rssiMeasures = readMeasureArray(rssi_addr);
    loopCounter = EEPROM.read(4) + 1;
  }
}

void writeValuesOnEEPROM(){
  Serial.println(F("Writing measures on EEPROM"));
  saveMeasureArray(temperatureMeasures, temp_addr);
  saveMeasureArray(humidityMeasures, humid_addr);
  saveMeasureArray(rssiMeasures, rssi_addr);
  if(eeprom_init != 1){
    EEPROM.write(0, 1);
    EEPROM.commit();
    delay(300);
  }
}

MeasureArrayHandler readMeasureArray(int mem_addr){
  int arraySize = EEPROM.read(mem_addr);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" <- ");
    Serial.println(arraySize);
  }
  mem_addr += 4;
  
  int elementsAdded = EEPROM.read(mem_addr);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" <- ");
    Serial.println(elementsAdded);
  }
  mem_addr += 4;

  float lastMean = EEPROM.read(mem_addr);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" <- ");
    Serial.println(lastMean);
  }
  mem_addr += 4;

  MeasureArrayHandler mh(arraySize, lastMean);

  for(int i=arraySize - elementsAdded; i< arraySize; i++){
    float measure = EEPROM.read(mem_addr) -100;
    if(MEMORY_DEBUG == 1){
      Serial.print(mem_addr);
      Serial.print(" <- ");
      Serial.println(measure);
    }
    mh.addElement(measure);
    mem_addr += 4;
  }

  return mh;
}

int saveMeasureArray(MeasureArrayHandler handler, int mem_addr){
  EEPROM.write(mem_addr, handler.arraySize);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" -> ");
    Serial.println(handler.arraySize);
  }
  
  mem_addr += 4;
  EEPROM.write(mem_addr, handler.elementsAdded);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" -> ");
    Serial.println(handler.elementsAdded);
  }
  
  mem_addr += 4;
  EEPROM.write(mem_addr, handler.lastMean);
  if(MEMORY_DEBUG == 1){
    Serial.print(mem_addr);
    Serial.print(" -> ");
    Serial.println(handler.lastMean);
  }
  
  mem_addr += 4;
  for(int i=handler.arraySize - handler.elementsAdded; i< handler.arraySize; i++){
    if(MEMORY_DEBUG == 1){
      Serial.print(mem_addr);
      Serial.print(" -> ");
      Serial.println(handler.measureArray[i]+100);
    }
    EEPROM.write(mem_addr, handler.measureArray[i]+100);
    mem_addr += 4;
  }
  EEPROM.commit();
  delay(300);

  return mem_addr;
}
