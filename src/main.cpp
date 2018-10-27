#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <avr/wdt.h>
#include "settings.h"
#include "time.h"

byte mac[] = MAC_ADDRESS;

EthernetClient client;
PubSubClient pubsub(client);
char time_epoch_topic[] = "time/epoch"

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, time_epoch_topic) == 0) {
    uint32 epoch = atol(payload);
    if (epoch > 1540000000) {
      time::set(epoch);
      Serial.print("Time set to ");
      Serial.println(epoch);
    }
  }
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  pinMode(A2, OUTPUT);
  analogWrite(A0, LOW);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac);
  Serial.print("Washing machine is at ");
  Serial.println(Ethernet.localIP());
  pubsub.setServer(MQTT_SERVER, 1883);
  pubsub.setCallback(mqtt_callback);
}

void idle(int seconds) {
  while (seconds--) {
      delay(1000);
      pubsub.loop();
  }
}

void report(float value) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["total"] = value;
  uint32 timestamp = time::get_current();
  if (timestamp > 0) {
    root["timestamp"] = timestamp;
  }

  String stream;
  root.printTo(stream);

  pubsub.publish("devices/deadbeeffeed", stream.c_str());
  Serial.println("MQTT message published");
}

void loop() {
  Ethernet.maintain();

  int samples = 100;
  float deltax = 0;
  float deltay = 0;
  float deltaz = 0;

  float x, y, z;
  float oldx, oldy, oldz;
  oldx = analogRead(A5);
  oldy = analogRead(A4);
  oldz = analogRead(A3);

  if (pubsub.connect("washing-machine")) {
    pubsub.subscribe(time_epoch_topic);

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

      delay(10);
    }

    idle(30); // hopefully we'll acquire a timestamp here

    report(deltax + deltay + deltaz);

    pubsub.disconnect();
    Serial.println("MQTT disconnected");
  }
}
