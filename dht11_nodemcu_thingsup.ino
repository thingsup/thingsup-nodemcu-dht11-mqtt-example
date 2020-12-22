#include "DHT.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>                             // Ardiono JSON v6 Library

DHT dht;

// Update these with values suitable for your network.
char* ssid = " ";                                   // WIFI SSID of network
char* password = " ";                               // WIFI password of the newtowk
const int sensor_pin = D1;                          // Data pin of the DHT11 sensor for data communication

const char* mqtt_server = "mqtt.thingsup.io";            // Thingsup Broker URL
const char* mqtt_password = " ";                          // Device Password set in the device addition stage in thingsup
const char* mqtt_username = " ";                          // Device Key set in the device addition stage in thingsup
String clientId = " ";                                    // Client ID set in the device addition stage in thingsup
unsigned int mqtt_port = 1883;                           // MQTT port for SSL connection in thingsup
String accountID = " ";

/**
 * SSL Certificate for Validation to Prevent Man-In-Middle Attack.
 */
 static const char digicert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

BearSSL::X509List cert(digicert);
  
long current_millis = 0;
BearSSL::WiFiClientSecure espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;


void setup()
{
  Serial.begin(115200);
  setup_wifi();
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)");
  espClient.setTrustAnchors(&cert); //Comment this if using espClient.setInsecure();
  setClock(); //Comment this if using espClient.setInsecure();
  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  dht.setup(sensor_pin);   
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(dht.getMinimumSamplingPeriod()*2); /* Delay of amount equal to sampling period */
  float humidity = dht.getHumidity();/* Get humidity value */
  float temperature = dht.getTemperature();/* Get temperature value */
  Serial.print(dht.getStatusString());/* Print status of communication */
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.println(dht.toFahrenheit(temperature), 1);/* Convert temperature to Fahrenheit units */
  if (millis() - current_millis > 7000)
  {
    current_millis = millis();
  if (client.connected())
    {
      Serial.print("Publishing message: ");
      String publish_topic = "/"  + accountID + "/" + "temp";   // The topic name should be always start with /accountID/
      String json;
      StaticJsonDocument<500> doc;
      JsonObject root = doc.to<JsonObject>();
      root["temp"]=temperature;
      root["humi"]=humidity;
      serializeJson(doc, json);
      
      if (client.publish((char*)publish_topic.c_str(), json.c_str()) == true)
      {
        Serial.println("Success");
        serializeJsonPretty(doc, Serial);
      }
    }
  }

}

void setup_wifi() {

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //Use this if client id is kept as * in Thingsup Dashboard
  //clientId = "mqttdevice-" + String(ESP.getChipId(), HEX);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    Serial.print("clientId : ");
    Serial.println(clientId);

    Serial.print("mqtt_user=");
    Serial.println(mqtt_username);
    Serial.print("mqtt_pass=");
    Serial.println(mqtt_password);

    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password))
    {

      Serial.println("connected");
      
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  }
}

void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
