#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Arduino.h>
//#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>
#include "max6675.h" //https://github.com/adafruit/MAX6675-library

#define MQTT_MAX_PACKET_SIZE 512
#include <PubSubClient.h>
#define FORMAT_LITTLEFS_IF_FAILED true

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float t, tF;
int thermoDO_0 = 19; // SO of  MAX6675 module to D19
int thermoCS_0 = 5; // CS of MAX6675 module to D5
int thermoCLK_0 = 18; // SCK of MAX6675 module to D18

MAX6675 thermocouple(thermoCLK_0, thermoCS_0, thermoDO_0);

float thermoFahrenheit;
const char* TEMPERATURE_KEY = "temperature";
Preferences preferences;
JSONVar readings;
JSONVar configs;

const char* ssid = "your-network-ssid";
const char* password = "your-network-password";
const char* mqtt_server = "your-mqtt-ip";
const char* mqtt_username = "your-mqtt-username";
const char* mqtt_password = "your-mqtt-password";


WiFiClient espClient;
PubSubClient client(espClient);

const int TEMPERATURE_ADJUST_DEFAULT = 0;
const char* TEMPERATURE_ADJUSTMENT_KEY = "tempAdjust_0";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 15000;

void reconnect() {
  while (!client.connected()) {
    // Use the connect function with username and password
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      // If you need to subscribe to topics upon connection, do it here
    } else {
      // If the connection fails, wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  Serial.begin(9600);
  Serial.println("Starting setup()");
  delay(1000);

  initLittleFS();
  delay(1000);

  initWiFi();
  delay(1000);

  client.setServer(mqtt_server, 1883);
  client.setBufferSize(512);
  delay(1000);

  preferences.begin("thermo", false);
  Serial.println("Stored Preferences for > thermo");
  delay(1000);

  // preferences will persist against reboots, but not reflashes. Provide a default value if not found
  int storedAdjustmentValue = preferences.getInt(TEMPERATURE_ADJUSTMENT_KEY, TEMPERATURE_ADJUST_DEFAULT);
  configs[TEMPERATURE_ADJUSTMENT_KEY] = storedAdjustmentValue; 
  Serial.println(storedAdjustmentValue);


  // Check if the key exists and directly handle the value numerically
  if (configs.hasOwnProperty(TEMPERATURE_ADJUSTMENT_KEY)) {
    // Attempt direct extraction as a numeric value, avoiding string conversion
    float adjustmentValue = static_cast<float>((int)configs[TEMPERATURE_ADJUSTMENT_KEY]);
    Serial.print("Stored value for key ");
    Serial.print(TEMPERATURE_ADJUSTMENT_KEY);
    Serial.print(": ");
    Serial.println(adjustmentValue, 2); // Print with two decimal places for clarity
  } else {
    Serial.println("Key not found.");
  }



  initWebSocket();
  delay(1000);

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("web resquest for root");
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "This is a test");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();

  delay(1000);
  initHomeAssistantDiscovery();

  delay(1000);
  initializeReadings(thermocouple.readFahrenheit() + static_cast<float>((int)configs[TEMPERATURE_ADJUSTMENT_KEY]));
}

const int numReadings = 10; // Number of readings to average
float readings2[numReadings]; // Array to store the readings
int readIndex = 0; // Current position in the array
float total = 0; // Sum of the readings
float average = 0; // The average

// Function to initialize the readings with the first actual temperature
void initializeReadings(float initialReading) {
  total = 0; // Reset total to 0
  for (int i = 0; i < numReadings; i++) {
    readings2[i] = initialReading; // Set all readings to the initial value
    total += initialReading; // Add to total
  }
  average = initialReading; // Initial average is the actual temperature
}

// Function to update the average whenever you get a new reading
float updateAverage(float newReading) {
  // Subtract the oldest reading from the total, and add the new one
  total = total - readings2[readIndex] + newReading;
  readings2[readIndex] = newReading;

  // Move to the next position in the array
  readIndex = (readIndex + 1) % numReadings;

  // Calculate the new average
  average = total / numReadings;

  return average; // Return the updated average
}

void initHomeAssistantDiscovery(){

  if (!client.connected()) {
      reconnect(); // Your function to reconnect to the MQTT broker
  }
  
  Serial.println("Starting HA Discovery...");
  // MQTT Discovery message
  StaticJsonDocument<512> discoveryDoc;
  discoveryDoc["name"] = "ESP32 Temperature Sensor";
  discoveryDoc["state_topic"] = "home/esp32/temperature";
  discoveryDoc["unit_of_measurement"] = "°F"; // or > \u00B0F
  discoveryDoc["value_template"] = "{{ value_json.temperature }}";
  discoveryDoc["device_class"] = "temperature";
  discoveryDoc["unique_id"] = "esp32_temperature_sensor_unique_id";

  String discoveryMessage;
  serializeJson(discoveryDoc, discoveryMessage);

  // Adjust this topic to match your Home Assistant's expected discovery topic format
  // Example topic format: homeassistant/sensor/esp32/temperature/config

  // testing start
  //String manualDiscoveryMessage = "{\"name\":\"ESP32 Temp Sensor\",\"state_topic\":\"home/esp32/temperature\",\"unit_of_measurement\":\"F\"}";
  //client.publish("homeassistant/sensor/esp32/temperature/config", manualDiscoveryMessage.c_str(), true);

  client.publish("homeassistant/sensor/esp32/temperature/config", discoveryMessage.c_str(), true);
 
  //String manualDiscoveryMessage = "{\"name\":\"ESP32 Temperature Sensor\",\"state_topic\":\"home/esp32/temperature\",\"unit_of_measurement\":\"\u00B0F\", \"value_template\", \"{{ value_json.temperature }}\", \"device_class\", \"temperature\", \"unique_id\", \"esp32_temperature_sensor_unique_id\"}";
  
  //client.publish("homeassistant/sensor/esp32/temperature/config", manualDiscoveryMessage.c_str(), true);

  // testing end

  //client.publish("homeassistant/sensor/esp32/temperature/config", discoveryMessage.c_str(), true);
  
  Serial.println("Finished HA Discovery");
}

void initLittleFS(){
    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");
        return;
    }
    else{
        Serial.println("Little FS Mounted Successfully");
    }
}

void initWiFi() {
  Serial.println("Connecting to WiFi..");
  IPAddress local_IP(192, 168, 86, 183);
  IPAddress gateway(192, 168, 86, 1);
  IPAddress subnet(255, 255, 255, 0);
  //IPAddress primaryDNS(8, 8, 8, 8);
  //IPAddress secondaryDNS(8, 8, 4, 4);
  if(!WiFi.config(local_IP, gateway, subnet)) { //, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String getSensorReadings() {


  // real sensor values
  t = thermocouple.readCelsius();
  tF = updateAverage(thermocouple.readFahrenheit() + static_cast<float>((int)configs[TEMPERATURE_ADJUSTMENT_KEY]));
  //Serial.print("\t Deg F = ");
  //Serial.println(tF);

  // Create a JSON document
  StaticJsonDocument<256> doc; // Adjust size as needed

  // Format the temperature with one decimal place
  char temperatureStr[8]; // Buffer to hold the formatted temperature string
  dtostrf(tF, 0, 1, temperatureStr); // Convert float to string with 1 decimal place

  // Add the formatted temperature string to the JSON document
  doc[TEMPERATURE_KEY] = temperatureStr;

  // Serialize JSON to string
  String jsonString;
  serializeJson(doc, jsonString);

  Serial.println(jsonString);

  return jsonString;



/*

  // Fake sensor values
  // Generate a random float between 0 and 100
  float thermoFahrenheit = (float)rand() / (float)(RAND_MAX) * 100;

  // Create a JSON document
  StaticJsonDocument<256> doc; // Adjust size as needed

  // Format the temperature with one decimal place
  char temperatureStr[8]; // Buffer to hold the formatted temperature string
  dtostrf(thermoFahrenheit, 0, 1, temperatureStr); // Convert float to string with 1 decimal place

  // Add the formatted temperature string to the JSON document
  doc[TEMPERATURE_KEY] = temperatureStr;

  // Serialize JSON to string
  String jsonString;
  serializeJson(doc, jsonString);

  Serial.println(jsonString);

  return jsonString;

*/
}

void notifyClients(String payload) {
  ws.textAll(payload);
}

void SendClientsConfiguration(){
  String jsonString = JSON.stringify(configs);
  notifyClients(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String message = String((char*)data);
    JSONVar jsonData = JSON.parse(message);

    if (jsonData.hasOwnProperty("event")) {
      String event = (const char*)jsonData["event"];
      Serial.print("event: ");
      Serial.println(event);

      if(event == "updateConfiguration"){ 
        String jsonString = JSON.stringify(jsonData["configuration"]);
        Serial.println(jsonString); // ie: {"thermo_adjustment":"88"}
        DynamicJsonDocument doc(256);
    
        // Deserialize the JSON string into the JsonDocument
        DeserializationError error = deserializeJson(doc, jsonString);
  
        if (error) {
            Serial.print("Parsing Failed! ");
            Serial.println(error.c_str());
            return;
        }
    
        int tk = doc[TEMPERATURE_ADJUSTMENT_KEY];

        Serial.println("Event Adjustment Value: ");
        Serial.println(tk);

        preferences.putInt(TEMPERATURE_ADJUSTMENT_KEY, tk);

        configs[TEMPERATURE_ADJUSTMENT_KEY] = tk; 
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      SendClientsConfiguration();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if ((millis() - lastTime) > timerDelay) {
    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings); 
    const char* messageCStr = sensorReadings.c_str();
    client.publish("home/esp32/temperature", messageCStr);  
    lastTime = millis();
  }
  ws.cleanupClients();
}
