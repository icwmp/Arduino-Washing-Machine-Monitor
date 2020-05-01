#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/pgmspace.h>
#include "settings.h"

OneWire wire(2); // PIN 2
DallasTemperature sensors(&wire);
DeviceAddress ds18b20_address;

EthernetClient client;
PubSubClient pubsub(client);

#define NUM_SENSORS 2
#define TOPIC_LENGTH 64
#define PAYLOAD_LENGTH 20

const char device_topic[] PROGMEM = "homeassistant/sensor/0x1587373390";
const char sensor_kind[][13] PROGMEM = { "/vibration", "/temperature" };
const char config_payload[][250] PROGMEM = { R"EOF(
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

void printAddress(DeviceAddress deviceAddress)
{ 
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(", ");
  }
  Serial.println("");
}

void configureAll() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    memset(topic, 0x0, TOPIC_LENGTH);
    strcat_P(topic, device_topic);
    strcat_P(topic, sensor_kind[i]);
    strcat(topic, "/config");
    if (pubsub.connect("washing-machine")) {
      Serial.println(topic);
      if (pubsub.publish_P(topic, config_payload[i], true)) {
        Serial.println("published.");
      } else {
        Serial.println("not published.");
      }
    }
  }
}

/* int freeRam () {
   extern int __heap_start, *__brkval;
   int v;
   return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
} */

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  sensors.begin();

  if (sensors.getDS18Count() == 0) {
    Serial.println(F("No DS18B20, looping"));
    for(;;);
  }
  sensors.getAddress(ds18b20_address, 0);
  sensors.setResolution(12);

  // Provide GND to the accelerometer module.
  pinMode(A2, OUTPUT);
  digitalWrite(A2, 0);

  while (Ethernet.linkStatus() == LinkOFF) {
    delay(1000);
    Serial.println(F("waiting for carrier..."));
  }
  
  if (Ethernet.begin(ds18b20_address) == 0) {
    Serial.println(F("dhcp error. stopping."));
    for(;;);
  }
  Serial.print("Washing machine is at ");
  Serial.println(Ethernet.localIP());
  pubsub.setServer(MQTT_SERVER, 1883);
  configureAll();
}

void reportVibrations(long value) {
  memset(topic, 0x0, TOPIC_LENGTH);
  strcat_P(topic, device_topic);
  strcat_P(topic, sensor_kind[0]);
  strcat(topic, "/state");
  memset(mqtt_payload, 0x0, PAYLOAD_LENGTH);
  ltoa(value, mqtt_payload, 10);

  Serial.println(topic);
  Serial.println(mqtt_payload);
  if (!pubsub.publish(topic, mqtt_payload)) {

  }
}

void reportTemperature(double value) {
  memset(topic, 0x0, TOPIC_LENGTH);
  strcat_P(topic, device_topic);
  strcat_P(topic, sensor_kind[1]);
  strcat(topic, "/state");
  memset(mqtt_payload, 0x0, PAYLOAD_LENGTH);
  dtostrf(value, 4, 2, mqtt_payload);

  Serial.println(topic);
  Serial.println(mqtt_payload);
  if (!pubsub.publish(topic, mqtt_payload)) {
    Serial.println("not published!");
  }
}

void loop() {
  Ethernet.maintain();

  int samples = 600; // ~1 min worth of samples
  float deltax = 0;
  float deltay = 0;
  float deltaz = 0;

  float x, y, z;
  float oldx, oldy, oldz;
  oldx = analogRead(A5);
  oldy = analogRead(A4);
  oldz = analogRead(A3);

  if (pubsub.connect("washing-machine")) {
    Serial.println("MQTT connected");
    while (samples--) {
      x = analogRead(A5);
      y = analogRead(A4);
      z = analogRead(A3);

      deltax += fabs(x - oldx);
      deltay += fabs(y - oldy);
      deltaz += fabs(z - oldz);

      oldx = x;
      oldy = y;
      oldz = z;

      delay(100);
      pubsub.loop();
    }

    reportVibrations(deltax + deltay + deltaz);
    float value = sensors.getTempC(ds18b20_address);
    if (value == DEVICE_DISCONNECTED_C) {
      Serial.println(F("Can't read temperature"));
    } else {
      reportTemperature(value);
    }
    
    pubsub.disconnect();
    Serial.println("MQTT disconnected");
  }
}
