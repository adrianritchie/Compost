/*-----( Import needed libraries )-----*/
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WifiManager.h>
#include <ArduinoJson.h>

#include <BlynkSimpleEsp8266_SSL.h>

/*-----( Declare Constants and Pin Numbers )-----*/
#define SENSOR_PIN 12  // Any pin 2 to 12 (not 13) and A0 to A5
#define WIFI_RESET_PIN 14 //connect high to reset

/*-----( Declare objects )-----*/
OneWire  oneWire(SENSOR_PIN);  // Create a 1-wire object
DallasTemperature sensors(&oneWire);

//Sensor addresses
DeviceAddress probe1 = { 0x28, 0xFF, 0x24, 0xA8, 0x52, 0x16, 0x04, 0x09 };
DeviceAddress probe2 = { 0x28, 0xFF, 0x6B, 0x65, 0x47, 0x16, 0x03, 0xD7 };
DeviceAddress probe3 = { 0x28, 0xFF, 0x23, 0xA0, 0x52, 0x16, 0x04, 0x44 };


//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "blynk.kodo.gg";
char mqtt_port[6] = "8443";
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);

//flag for saving data
bool shouldSaveConfig = false;

#define BLYNK_PRINT Serial // Defines the object that is used for printing
#define BLINK_DEBUG
BlynkTimer timer;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readConfig();

void saveConfig();

void connectWifi();

void printTemperature(DeviceAddress deviceAddress, int vPin);

void timerCallback();

void setup() {
  Serial.begin(9600);

  pinMode(WIFI_RESET_PIN, INPUT);
  pinMode(SENSOR_PIN, INPUT);

  delay(500);



  readConfig();
  connectWifi();
  saveConfig();

  int port_number;
  port_number = atoi(mqtt_port);

  Serial.print("Blynk token ");
  Serial.println(blynk_token);
  Serial.print("Blynk server ");
  Serial.println(mqtt_server);
  Serial.print("Blynk port number ");
  Serial.println(port_number);

  Blynk.config(blynk_token, mqtt_server, port_number);
  Blynk.connect();
  timer.setInterval(1000, timerCallback);


  sensors.setResolution(probe1, 10);
  sensors.setResolution(probe2, 10);
  sensors.setResolution(probe3, 10);
}//--(end setup )---

void loop() {
  if (!Blynk.connected()) {
    Serial.println("Blynk not connected to server");
  }

  Blynk.run();
  timer.run();
}

void timerCallback() {
  delay(1000);
  Serial.println();
  Serial.print("Number of Devices found on bus = ");
  Serial.println(sensors.getDeviceCount());
  Serial.print("Getting temperatures... ");
  Serial.println();

  // Command all devices on bus to read temperature
  sensors.requestTemperatures();

  Serial.print("Probe 01 temperature is:   ");
  printTemperature(probe1, 5);
  Serial.println();

  Serial.print("Probe 02 temperature is:   ");
  printTemperature(probe2, 6);
  Serial.println();

  Serial.print("Probe 03 temperature is:   ");
  printTemperature(probe3, 7);
  Serial.println();

}//--(end main loop )---

void printTemperature(DeviceAddress deviceAddress, int vPin) {

  float tempC = sensors.getTempC(deviceAddress);

  if (tempC == -127.00)
  {
    Serial.print("Error getting temperature  ");
  }
  else
  {
    Serial.print("C: ");
    Serial.print(tempC);
    Serial.print(" F: ");
    Serial.print(DallasTemperature::toFahrenheit(tempC));
    Blynk.virtualWrite(vPin, tempC);
    Serial.print(" sent to Blynk");
  }
}// End printTemperature

void readConfig() {

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void saveConfig() {
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void connectWifi() {
    //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //for testing:
  Serial.print("WIFI_RESET_PIN ");
  Serial.println(digitalRead(WIFI_RESET_PIN));
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("Resetting WiFi config...");
    wifiManager.resetSettings();
  }

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

}
//*********( THE END )***********
