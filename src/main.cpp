#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <avr/wdt.h>
#include "settings.h"

byte mac[] = MAC_ADDRESS;

EthernetClient client;
PubSubClient pubsub(client);
const char base_topic[] = "homeassistant/sensor/0x1587373390/vibration";
const char config_payload[] = R"EOF(
{"name": "Washing Machine",
"unique_id": "0x1587373390-vibration",
"device": {"identifiers": "0x1587373390"},
"state_topic": "homeassistant/sensor/0x1587373390/vibration/state",
"unit_of_measurement": "rpm"}
)EOF";

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  // Provide GND to the accelerometer module.
  pinMode(A2, OUTPUT);
  digitalWrite(A2, 0);

  while (Ethernet.linkStatus() == LinkOFF) {
    delay(1000);
    Serial.println("waiting for carrier...");
  }
  if (Ethernet.begin(mac) == 0) {
    Serial.println("problem with dhcp");
    for(;;);
  }
  Serial.print("Washing machine is at ");
  Serial.println(Ethernet.localIP());
  pubsub.setServer(MQTT_SERVER, 1883);
  String config_topic = String(base_topic) + "/config";
  if (pubsub.connect("washing-machine")) {
    Serial.println(config_topic.c_str());
    pubsub.publish(config_topic.c_str(), config_payload);
    Serial.println("pushed configuration");
  }
}

void report(long value) {
  String str_value = String(value);
  String state_topic = String(base_topic) + "/state";
  pubsub.publish(state_topic.c_str(), str_value.c_str());
  Serial.println("MQTT message published");
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

    report(deltax + deltay + deltaz);

    pubsub.disconnect();
    Serial.println("MQTT disconnected");
  }
}
