#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <avr/wdt.h>
#include "settings.h"

byte mac[] = MAC_ADDRESS;

EthernetClient client;
PubSubClient pubsub(client);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  pinMode(A2, OUTPUT);
  analogWrite(A0, LOW);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac);
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  pubsub.setServer(MQTT_SERVER, 1883);
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

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["total"] = deltax + deltay + deltaz;

  String stream;
  root.printTo(stream);

  if (pubsub.connect("washing-machine")) {
    Serial.println("MQTT conntected");
    pubsub.publish("/devices/deadbeeffeed", stream.c_str());
    Serial.println("MQTT message published");
    pubsub.disconnect();
    Serial.println("MQTT disconnected");
  }

  delay(30000);
}
