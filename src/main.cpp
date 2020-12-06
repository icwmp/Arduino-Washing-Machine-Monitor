#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "settings.h"

WiFiClient client;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
PubSubClient pubsub(client);

#define NUM_SENSORS 2
#define TOPIC_LENGTH 64
#define PAYLOAD_LENGTH 20
#define ONEWIRE_PIN D3
#define CLIENT_NAME "washing-machine"
#define NUMBER_OF_SAMPLES 1000  // 1560ms for 1000 samples
#define NUMBER_OF_RUNS 120000 / 1560 // 2 mins worth of samples

OneWire wire(ONEWIRE_PIN); // PIN 2
DallasTemperature sensors(&wire);
DeviceAddress ds18b20_address;

void reset() {
  Serial.println("Resetting!");
  Serial.flush();
  ESP.reset();
}

const char device_topic[] = "homeassistant/sensor/0x1587373390";
const char sensor_kind[][13] = { "/vibration", "/temperature" };
const char config_payload[][250] = { R"EOF(
{"name": "Washing Machine Vibrations",
"unique_id": "0x1587373390-vibration",
"device": {"identifiers": "0x1587373390"},
"state_topic": "homeassistant/sensor/0x1587373390/vibration/state",
"unit_of_measurement": "rpm"}
)EOF",
R"EOF(
{"name": "Wash Room Temperature",
"unique_id": "0x1587373390-t",
"device": {"identifiers": "0x1587373390"},
"device_class": "temperature",
"unit_of_measurement": "°C",
"state_topic": "homeassistant/sensor/0x1587373390/temperature/state"}
)EOF" };

// buffers used across all functions
char mqtt_payload[PAYLOAD_LENGTH]; 
char topic[TOPIC_LENGTH];

void configureAll() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    memset(topic, 0x0, TOPIC_LENGTH);
    strcat(topic, device_topic);
    strcat(topic, sensor_kind[i]);
    strcat(topic, "/config");
    if (pubsub.connect(CLIENT_NAME)) {
      Serial.println(topic);
      if (pubsub.publish(topic, config_payload[i], true)) {
        Serial.println("published.");
      } else {
        Serial.println("not published.");
      }
    }
  }
}

WiFiManager wifiManager;

void setup_esp8266() {
  wifiManager.setConfigPortalTimeout(60);
  wifiManager.setConnectTimeout(60);
  wifiManager.setMinimumSignalQuality(20);

  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    reset();
  }
  Serial.print("Washing machine is at ");
  Serial.println(WiFi.localIP());
}

void setup_sensors() {
  // ADXL345
  if (!accel.begin()) {
    Serial.println("Accelerometer not detected");
    reset();
  }
  accel.setRange(ADXL345_RANGE_4_G);
  accel.setDataRate(ADXL345_DATARATE_3200_HZ);

  // DS18B20
  sensors.begin();
  if (sensors.getDS18Count() == 0) {
    Serial.println("No DS18B20, looping");
    reset();
  }
  sensors.getAddress(ds18b20_address, 0);
  sensors.setResolution(12);
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  setup_esp8266();
  setup_sensors();

  pubsub.setServer(MQTT_SERVER, 1883);
  configureAll();
}

void reportVibrations(float value) {
  memset(topic, 0x0, TOPIC_LENGTH);
  strcat(topic, device_topic);
  strcat(topic, sensor_kind[0]);
  strcat(topic, "/state");
  memset(mqtt_payload, 0x0, PAYLOAD_LENGTH);
  dtostrf(value, 5, 3, mqtt_payload);

  Serial.println(topic);
  Serial.println(mqtt_payload);
  if (!pubsub.publish(topic, mqtt_payload)) {
    Serial.println("Can't report vibrations");
    reset();
  }
}

void reportTemperature(double value) {
  memset(topic, 0x0, TOPIC_LENGTH);
  strcat(topic, device_topic);
  strcat(topic, sensor_kind[1]);
  strcat(topic, "/state");
  memset(mqtt_payload, 0x0, PAYLOAD_LENGTH);
  dtostrf(value, 4, 2, mqtt_payload);

  Serial.println(topic);
  Serial.println(mqtt_payload);
  if (!pubsub.publish(topic, mqtt_payload)) {
    Serial.println("Can't report temperature");
    reset();
  }
}

inline float runSampling(int howmany) {
  float sum = 0;
  float deltax = 0;
  float deltay = 0;
  float deltaz = 0;

  float x, y, z;
  float oldx, oldy, oldz;
  sensors_event_t event; 

  accel.getEvent(&event);
  oldx = event.acceleration.x;
  oldy = event.acceleration.y;
  oldz = event.acceleration.z;

  for (int n = 0; n < howmany; n++) {
      ESP.wdtFeed(); // should prevent the sensor hanging the ESP
      accel.getEvent(&event);

      x = event.acceleration.x;
      y = event.acceleration.y;
      z = event.acceleration.z;

      deltax += fabs(x - oldx);
      deltay += fabs(y - oldy);
      deltaz += fabs(z - oldz);

      oldx = x;
      oldy = y;
      oldz = z;

      sum += deltax + deltay + deltaz;
  }
  return sum;
}

float sampleVibrations(int nruns, bool debug) {
  unsigned long t = millis();
  if (debug) {
    Serial.print("Starting sampling ... ");
  }

  float sum = 0;
  for (int n = 0; n < nruns; n++) {
    sum += runSampling(NUMBER_OF_SAMPLES);
  }

  if (debug) {
    t = millis() - t;
    Serial.print("finished in ");
    Serial.println(t);
  }
  
  return sum / nruns;
}

void loop() {
  sensors.requestTemperatures();
  float averagedSamples = sampleVibrations(NUMBER_OF_RUNS, true);
  
  pubsub.loop();

  Serial.println(log10(averagedSamples));

  if (pubsub.connect(CLIENT_NAME)) {
    Serial.println("MQTT connected");

    reportVibrations(log10(averagedSamples));
    float value = sensors.getTempC(ds18b20_address);
    if (value == DEVICE_DISCONNECTED_C) {
      Serial.println("Can't read temperature");
    } else {
      reportTemperature(value);
    }
    
    pubsub.disconnect();
    Serial.println("MQTT disconnected");
  } else {
    reset();
  }
}
