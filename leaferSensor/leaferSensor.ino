#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "DHTesp.h"

const char* ssidDevice = "leaferSensor";

int DHTpin = 14;
int sensorPin = 34;
int sensorValue = 0;
int valuePercent = 0;
float air = 0.0;
float temperature = 0.0;

AsyncWebServer webServer(80);
DHTesp dht;

char ssid[50];
char password[50];
char token[300];
char plantCollectionId[50];
char sensorId[50];

bool isReady = false;

//const char* queryUrl = "https://leafer-rest-api-prod.herokuapp.com/sensor";
const char* queryUrl = "http://192.168.1.29:3000/sensor";

//const char* queryUrlSensorData = "https://leafer-rest-api-prod.herokuapp.com/sensor-data";
const char* queryUrlSensorData = "http://192.168.1.29:3000/sensor-data";

void createSensor(){
  vTaskDelay(1000);
  
  HTTPClient httpCreate;
  String postMessage;
  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  doc["plantCollectionId"] = String(plantCollectionId).toInt();
  serializeJsonPretty(doc, postMessage);
  serializeJsonPretty(doc, Serial);
  httpCreate.begin(String(queryUrl));
  httpCreate.addHeader("Content-Type", "application/json");
  httpCreate.addHeader("Authorization", "Bearer " + String(token));
  httpCreate.addHeader("Host", "leafer-rest-api-prod.herokuapp.com");
  int httpCreateCode = httpCreate.POST(postMessage);
  if (httpCreateCode > 0) {
    Serial.println("Server reached and responded to POST request.");
    Serial.println();
    Serial.println(httpCreateCode);
    if (httpCreateCode == HTTP_CODE_CREATED) {
      Serial.print("Server responded with requested payload: ");
    }
    else {
      Serial.println("Server error");
      
    }
    String payload = httpCreate.getString();
    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, payload);
    JsonObject res = jsonBuffer.as<JsonObject>();
    String sensorString = res["id"];
    sensorString.toCharArray(sensorId, 50);
    Serial.println(sensorId);
    Serial.println(payload);
  }
  else{
    Serial.println("Server could not be reached or server did not reply to POST request.");
  }
  
  httpCreate.end(); 
}



void sendData() {

  air = dht.getHumidity();
  temperature = dht.getTemperature();
  sensorValue = analogRead(sensorPin);
  valuePercent = 100 - map(sensorValue, 0, 4095, 0, 100);

  String postMessage;
  HTTPClient httpUpdate;
  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  doc["sensorId"] = String(sensorId).toInt();
  doc["groundHumidity"] = valuePercent;
  doc["airHumidity"] = (int) air;
  doc["temperature"] = (int) temperature;
  
  serializeJsonPretty(doc, postMessage);
  serializeJsonPretty(doc, Serial);

  httpUpdate.begin(queryUrlSensorData);
  httpUpdate.addHeader("Content-Type", "application/json");
  httpUpdate.addHeader("Authorization", "Bearer " + String(token));
  int httpUpdateCode = httpUpdate.POST(postMessage);
  if (httpUpdateCode > 0) {
    Serial.println("Server reached and responded to POST request.");
    Serial.println();
    Serial.println(httpUpdateCode);
    if (httpUpdateCode == HTTP_CODE_CREATED) {
      Serial.print("Server responded with requested payload: ");
      String payload = httpUpdate.getString();
      Serial.println(payload);
    }
    else {
      String payload = httpUpdate.getString();
      if(payload.indexOf("Sensor disabled")>0){
        Serial.println("Sensor disabled");
        Serial.println("Switching to pairing mode");
        ESP.restart();
      }
      Serial.println("Server error");
    }
  }
  else{
    Serial.println("Server could not be reached or server did not reply to POST request.");
  }
  httpUpdate.end();
  vTaskDelay(5000);
}

void setup() {
  Serial.begin(115200);
  dht.setup(DHTpin, DHTesp::DHT11);
  Serial.println();
  Serial.println("Configuring access point...");
  WiFi.softAP(ssidDevice, "123456789");
  IPAddress ipDevice = WiFi.softAPIP();
  Serial.print("Device IP address: ");
  Serial.println(ipDevice);

  webServer.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request){

    if(isReady == false) {
      int paramsNr = request->params();
      Serial.println(paramsNr);
      for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->name().equals("ssid")){
          p->value().toCharArray(ssid, 50);
        }
        if(p->name().equals("pwd")){
          p->value().toCharArray(password, 50);
        }
        if(p->name().equals("token")){
          p->value().toCharArray(token, 300);
        }
        if(p->name().equals("plantCollectionId")){
          p->value().toCharArray(plantCollectionId, 50);
        }
        if(p->name().equals("sensorId")){
          p->value().toCharArray(sensorId, 50);
        }
        Serial.print("Param name: ");
        Serial.println(p->name());
        Serial.print("Param value: ");
        Serial.println(p->value());
        Serial.println("------");
      }
  
      Serial.println();
      Serial.println();
      Serial.print("Connecting to ");
      Serial.println(ssid);
      Serial.print(" with userId ");
      Serial.println(token);
      
      AsyncWebServer wifiServer(80);
      
      WiFi.begin(ssid, password);
  
      while (WiFi.status() != WL_CONNECTED) {
          vTaskDelay(1000);
          Serial.print(".");
      }
  
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    
      
  
      wifiServer.begin();
      createSensor();
      isReady = true;
      request->send(200, "text/plain", "sensor created and connected");
    }
    else{
      request->send(200, "text/plain", "sensor already created and connected");
    }
    
  });
  webServer.begin();
}

void loop() {
  vTaskDelay(5000);
  if(isReady){
    sendData();
  }
}
