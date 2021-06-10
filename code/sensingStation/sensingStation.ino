
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
#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27   // display address on I2C bus
#define DHTTYPE DHT11   // sensor type DHT 11
#define BUTTON_DEBOUNCE_DELAY 20  // Button debounce

// Software constants
#define SIZE_MEASURES_AVG_ARRAY    6         // Store last 6 values measured


// Initialize objects for sensors
DHT dht = DHT(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object

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
boolean lcdStatus = true;

// Global variables for values
MeasureArrayHandler temperatureMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler humidityMeasures(SIZE_MEASURES_AVG_ARRAY);
MeasureArrayHandler rssiMeasures(SIZE_MEASURES_AVG_ARRAY);
float heatIndex;
String macAddr;




void setup() {
  // Faccio il setup
  initializeHardware();

  while (!Serial) { }

  // Inizialize EPROM
  short eeprom_init = 0;
  int eprom_size = 3 * 40 + sizeof(eeprom_init) + 2;
  EEPROM.begin(eprom_size);
  delay(100);

  // Connessione Wifi
  connectWifi();
  macAddr = WiFi.macAddress();

  // Connessione MQTT
  connectToMQTTBroker();

  // Discovery
  performDiscovery();
  while(!discoveryEnded){mqttClient.loop(); delay(5);}
  Serial.println("Finished discovery procedure");

  // Subscribe topics
  humidityStatusReceived = false;
  temperatureStatusReceived = false;
  subscribeTopicsMQTT();

  // Aspetto di ricevere notizie
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

  int temp_addr = sizeof(eeprom_init)+2;
  int humid_addr = temp_addr + 40;
  int rssi_addr = humid_addr + 40;
  
  // Leggo dalla EPROM
  eeprom_init = EEPROM.read(0);
  Serial.print(F("Reading from EEPROM, init value: "));
  Serial.println(eeprom_init);
  if(eeprom_init != 1){
    Serial.println("Nothing has been initialized yet");
  } else {
    temperatureMeasures = readMeasureArray(temp_addr);
    humidityMeasures = readMeasureArray(humid_addr);
    rssiMeasures = readMeasureArray(rssi_addr);
  }

  // TEST
  temperatureMeasures.printArray();
  humidityMeasures.printArray();
  rssiMeasures.printArray();
  
  // Leggo i valori dai sensori e aggiorno gli oggetti
  Serial.println("Reading values from sensors");
  readSensors();

  
  temperatureMeasures.printArray();
  humidityMeasures.printArray();
  rssiMeasures.printArray();
  
  // Lo scrivo sulla EPROM
  Serial.println("Writing measures on EEPROM");
  saveMeasureArray(temperatureMeasures, temp_addr);
  saveMeasureArray(humidityMeasures, humid_addr);
  saveMeasureArray(rssiMeasures, rssi_addr);
  if(eeprom_init != 1){
    EEPROM.write(0, 1);
    EEPROM.commit();
  }
  delay(200);

  // Ogni 3x loop, loggo sul DB la media (contatore anche qui)
  communicateValues();

  // Sleep di 10 minuti
  Serial.println("Going deep sleep for 10 minutes");
  ESP.deepSleep(5e6);
  //ESP.deepSleep(600e6);
  
}

void readSensors(){
  if(tempStatus)
    readTemperatureDHT();

  if(humidStatus)
    readHumidityDHT();

  int rssiValue= WiFi.RSSI();
  rssiMeasures.addElement(rssiValue);
}

void communicateValues(){
  if(humidStatus && tempStatus)
      calculateHeatIndex();
  
    // Update LCD
    if(lcdStatus){
      lcd.setBacklight(255);
      updateLCD();
    }
    else{
      lcd.setBacklight(0);
    }

    // Write temperature and humidity on MQTT
    communicateValuesMQTT(temperatureMeasures.getAverage(), humidityMeasures.getAverage());

    // Update influxDB instance
    writeOnDB();
}

void loop() {}

void initializeHardware(){
  Serial.begin(115200);

  mqttSetup();
  WiFi.mode(WIFI_STA);
  
  setupLCD();
  
  dht.begin();
  
  pinMode(CRIT_LED, OUTPUT);
  digitalWrite(CRIT_LED, HIGH);
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void readTemperatureDHT(){
  delay(2000);
  float tempValue = dht.readTemperature();
  Serial.println("Temp: " + String(tempValue) +"C");
  temperatureMeasures.addElement(tempValue);
}

void readHumidityDHT(){
  delay(2000);
  float humidValue = dht.readHumidity();
  Serial.println("Humidity: " + String(humidValue) +"%");
  humidityMeasures.addElement(humidValue);
}

void calculateHeatIndex(){
  heatIndex = dht.computeHeatIndex(temperatureMeasures.getAverage(), humidityMeasures.getAverage(), false);
  Serial.println("PercTemp: " + String(heatIndex) +"C");
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

MeasureArrayHandler readMeasureArray(int mem_addr){
  int arraySize = EEPROM.read(mem_addr);
  Serial.print(mem_addr);
  Serial.print(" <- ");
  Serial.println(arraySize);
  mem_addr += 4;
  
  int elementsAdded = EEPROM.read(mem_addr);
  Serial.print(mem_addr);
  Serial.print(" <- ");
  Serial.println(elementsAdded);
  mem_addr += 4;

  float lastMean = EEPROM.read(mem_addr);
  Serial.print(mem_addr);
  Serial.print(" <- ");
  Serial.println(lastMean);
  mem_addr += 4;

  MeasureArrayHandler mh(arraySize, lastMean);

  for(int i=arraySize - elementsAdded; i< arraySize; i++){
    float measure = EEPROM.read(mem_addr) -100;
  Serial.print(mem_addr);
  Serial.print(" <- ");
  Serial.println(measure);
    mh.addElement(measure);
    mem_addr += 4;
  }

  return mh;
}

int saveMeasureArray(MeasureArrayHandler handler, int mem_addr){
  EEPROM.write(mem_addr, handler.arraySize);
  Serial.print(mem_addr);
  Serial.print(" -> ");
  Serial.println(handler.arraySize);
  
  mem_addr += 4;
  EEPROM.write(mem_addr, handler.elementsAdded);
  Serial.print(mem_addr);
  Serial.print(" -> ");
  Serial.println(handler.elementsAdded);
  
  mem_addr += 4;
  EEPROM.write(mem_addr, handler.lastMean);
  Serial.print(mem_addr);
  Serial.print(" -> ");
  Serial.println(handler.lastMean);
  
  mem_addr += 4;
  for(int i=handler.arraySize - handler.elementsAdded; i< handler.arraySize; i++){
    Serial.print(mem_addr);
    Serial.print(" -> ");
    Serial.println(handler.measureArray[i]+100);
    EEPROM.write(mem_addr, handler.measureArray[i]+100);
    mem_addr += 4;
  }
  EEPROM.commit();
  delay(200);

  return mem_addr;
}
