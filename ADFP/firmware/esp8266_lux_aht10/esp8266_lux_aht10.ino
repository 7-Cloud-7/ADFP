#include <BH1750.h>
#include <Adafruit_AHT10.h>


#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "secrets.h"

BH1750 lightMeter;

Adafruit_AHT10 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length){
  
}


bool aht10running = true;

void setup() {
  Serial.begin(115200);
  Serial.println();
  
  
  Wire.begin(D3,D4);

  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
  Serial.println(F("BH1750 Init OK"));

  if (!aht.begin()) {
    Serial.println("Failed to find AHT10 chip");
    aht10running = false;
  }

  aht_temp = aht.getTemperatureSensor();
  aht_humidity = aht.getHumiditySensor();

  Serial.println(F("Wi-Fi init"));
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  delay(1000);
}


int lightSensorMTReg = 69;

// Returns lux, auto adjusts MTReg
float readLightSensor(){
  float lux = lightMeter.readLightLevel(); 

  if(lux < 0.f){
    return -1.f;
  } else {
    if(lux > 40000.f){
      lightMeter.setMTreg(32);
      lightSensorMTReg = 32;
    } else {
      if(lux > 10.f){
        lightMeter.setMTreg(69);
        lightSensorMTReg = 69;
      } else {
        if(lux <= 10.f){
          lightMeter.setMTreg(138);
          lightSensorMTReg = 138;
        }
      }
    }
  }
  
  return lux;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266-Luxometer")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("mqtt-light", "Connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void loop() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.println(F("WiFi down, rebooting..."));
    ESP.restart();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  

  float currentLux = readLightSensor();
  bool lightError  = false;
  
  if(currentLux < 0.f){
    Serial.println(F("luxometer error condition detected!"));
    lightError = true;
  }

  DynamicJsonDocument doc(1024);
  doc["now"]    = millis();
  doc["uptime"] = millis() / 1000.f;
  
  doc["BH1750"]["lux"]   = currentLux;
  doc["BH1750"]["MTreg"] = lightSensorMTReg;
  doc["BH1750"]["error"] = lightError;

  // Read humidity, temperature
  if(aht10running){
    sensors_event_t humidity;
    sensors_event_t temp;
    aht_humidity->getEvent(&humidity);
    aht_temp->getEvent(&temp);


    doc["AHT10"]["temperature"] = temp.temperature;
    doc["AHT10"]["humidity"]    = humidity.relative_humidity;
    doc["AHT10"]["error"]       = false;
  } else {
    doc["AHT10"]["temperature"] = -1;
    doc["AHT10"]["humidity"]    = -1;
    doc["AHT10"]["error"]       = true;
  }


  char jsonString[1024];
  serializeJsonPretty(doc, jsonString);


  Serial.println(jsonString);
  client.publish("mqtt-light", jsonString);
  
  
  delay(500);
}
