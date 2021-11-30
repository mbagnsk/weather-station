 #include <WiFi.h>
#include <HTTPClient.h>
#include <OneWireNg.h>
#include <DallasTemperature.h>

#define RELAY_PIN 32
#define ONE_WIRE_BUS 33
#define ANEMOMETR 34
#define RED_LIGHT LOW
#define GREEN_LIGHT HIGH

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* ssid = "Orange_Swiatlowod_E300";
const char* password = "kilocebuli";

const String AzureIoTHubURI = "https://WSIoTHub.azure-devices.net/devices/ESP32WeatherStation/messages/events?api-version=2020-03-13";
const String AzureIoTHubAuth= "SharedAccessSignature sr=WSIotHub.azure-devices.net%2Fdevices%2FESP32WeatherStation&sig=LpoalU302%2BrNWSug0JefUyE55ZN3HpKGVweo2DsnE%2BI%3D&se=1640268317";

int iteratorWiFi = 0;
const int maxIteratesWiFi = 40;

int iteratorMeasurement = 0;
int const minute = 60; // minute is a 60 seconds
int const period = 15; // 15 minutes - period after which the data would be send to Azure IoT Hub
int const measurementPerMinute = 12; // measure every 5 seconds if it's 12
int const measurementInterval = minute / measurementPerMinute * 1000; // in ms
const int maxIteratesMeasurement = period * measurementPerMinute;

const int windSpeedLimit = 16.66; // 16.66 m/s = 60km/h 
const int timeout = 300000; // = 300 seconds = 5 minutes

double temperature1C;
double temperature2C;
double windSpeed;
double maxMeasuredWindSpeed;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ONE_WIRE_BUS, INPUT);
  digitalWrite(RELAY_PIN, GREEN_LIGHT);
  sensors.begin();
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while((WiFi.status() != WL_CONNECTED) && (iteratorWiFi < maxIteratesWiFi)){
    delay(500);
    Serial.print(".");
    iteratorWiFi = iteratorWiFi + 1;
  } 
}

void loop() {
  // put your main code here, to run repeatedly: 
  iteratorWiFi = 0;
  iteratorMeasurement = 0;

  temperature1C = -99;
  temperature2C = -99;
  windSpeed = -99;
  maxMeasuredWindSpeed = -99;

  digitalWrite(RELAY_PIN, GREEN_LIGHT);
  
  while (iteratorMeasurement < maxIteratesMeasurement) {
    windSpeed = GetWindSpeed(ANEMOMETR);
    if (windSpeed > maxMeasuredWindSpeed){
      maxMeasuredWindSpeed = windSpeed;
    }
    if (maxMeasuredWindSpeed >=  windSpeedLimit){
      Serial.println("Przekroczono dopuszczalny limit");
      digitalWrite(RELAY_PIN, RED_LIGHT);
      break;    
    }
    iteratorMeasurement = iteratorMeasurement + 1;
    Serial.println(iteratorMeasurement);
    delay(measurementInterval);
  }
  
  temperature1C = GetTemperature(0);
  temperature2C = GetTemperature(1);
  String PostData="{ \"DeviceId\":\"esp32\",\"temperature1\":"+String(temperature1C)+",\"temperature2\":"+String(temperature2C)+",\"windSpeed\":"+String(maxMeasuredWindSpeed)+" }";
  if(WiFi.status() == WL_CONNECTED){
    int returnCode = RestPostData(AzureIoTHubURI, AzureIoTHubAuth, PostData);  
    Serial.println(returnCode);  
  }
  else {
    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while(WiFi.status() != WL_CONNECTED && iteratorWiFi < maxIteratesWiFi){
      delay(500);
      Serial.print(".");
      iteratorWiFi = iteratorWiFi + 1;
    }
    if(WiFi.status() == WL_CONNECTED){
      int returnCode = RestPostData(AzureIoTHubURI, AzureIoTHubAuth, PostData);
      Serial.println(returnCode);    
    }     
  }

  while (maxMeasuredWindSpeed >= windSpeedLimit){
    Serial.println("Przekroczono dopuszczalny limit");
    delay(timeout);
    windSpeed = -99;
    maxMeasuredWindSpeed = -99;
    for (iteratorMeasurement = 0; iteratorMeasurement < maxIteratesMeasurement/3 ; iteratorMeasurement++){
      windSpeed = GetWindSpeed(ANEMOMETR);
      if(windSpeed > maxMeasuredWindSpeed){
        maxMeasuredWindSpeed = windSpeed;
      }
      Serial.println(windSpeed);
      delay(measurementInterval);
    }
    temperature1C = GetTemperature(0);
    temperature2C = GetTemperature(1);
    String PostData="{ \"DeviceId\":\"esp32\",\"temperature1\":"+String(temperature1C)+",\"temperature2\":"+String(temperature2C)+",\"windSpeed\":"+String(maxMeasuredWindSpeed)+" }";
    if(WiFi.status() == WL_CONNECTED){
      int returnCode = RestPostData(AzureIoTHubURI, AzureIoTHubAuth, PostData);
      Serial.println(returnCode);    
    } 
    else {
      WiFi.begin(ssid, password);
      Serial.println("Connecting");
      while(WiFi.status() != WL_CONNECTED && iteratorWiFi < maxIteratesWiFi){
        delay(500);
        Serial.print(".");
        iteratorWiFi = iteratorWiFi + 1;
      }
      if(WiFi.status() == WL_CONNECTED){
        int returnCode = RestPostData(AzureIoTHubURI, AzureIoTHubAuth, PostData);
        Serial.println(returnCode);    
      }
    }
  }
}

int RestPostData(String URI, String Authorization, String PostData){
  HTTPClient http;
  http.begin(URI);
  http.addHeader("Authorization",Authorization);
  http.addHeader("Content-Type", "application/atom+xml;type=entry;charset=utf-8");
  int returnCode = http.POST(PostData);
  if(returnCode >= 400 && returnCode <= 499 ){
    Serial.println("RestPostData: Error sending data: "+String(http.errorToString(returnCode).c_str()));
  }
  http.end();
  return returnCode;
}

double GetTemperature(int i){
  // returns a value of temperature read by i temperature sensor 
  // returns value in Celsius scale
  double temperature = -127; // -127: error value
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(i);
  return temperature;
}

double GetWindSpeed(int inputPin){
  // returns a value of wind speed read by analog anemometr 
  // returns value in m/s
  int windSensorValue = analogRead(inputPin);
  double outVoltage = windSensorValue * (3.3/4095); // 3.3V - max input from anemometr, 3.3V coresponds to 4095  
  double windSpeed = outVoltage * 9.09;
  return windSpeed;
}
