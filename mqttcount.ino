// (C) folkert van heusden <mail@vanheusden.com>

#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"         // https://github.com/tzapu/WiFiManager

#include <PubSubClient.h>

char mqtt_server[128] = "10.208.11.30"; // default value

WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

#include <TM1637Display.h>
// FIXME CHANGE THIS FOR YOUR HARDWARE, ASSUMES A TM1637 DISPLAY
#define CLK 12
#define DIO 14
//#define CLK 5
//#define DIO 4
//#define CLK D2
//#define DIO D1

TM1637Display display(CLK, DIO);

static ESP8266WebServer *server = NULL;

uint16_t counts[61];

void configModeCallback(WiFiManager *myWiFiManager) {
	Serial.println(F("Entered config mode"));
	Serial.println(WiFi.softAPIP());
	Serial.println(myWiFiManager->getConfigPortalSSID());
}

volatile unsigned long int detectedCount = 0;

void handleRoot() {
	char buffer[128] = "?";
	snprintf(buffer, sizeof buffer, "%ld", detectedCount);
	buffer[sizeof buffer - 1] = 0x00;

	server -> send(200, "text/plain", buffer);
}

void reboot() {
	Serial.println(F("Reboot"));
	Serial.flush();
	delay(100);
	ESP.restart();
	delay(1000);
}

void setupWifi() {
	WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
	wifiManager.setTimeout(120);
	wifiManager.setAPCallback(configModeCallback);
	wifiManager.addParameter(&custom_mqtt_server);

	if (!wifiManager.autoConnect("mqttcount"))
		reboot();
}

int8_t prevSec = -1;
void callback(char* topic, byte* payload, unsigned int length) {
	int8_t secNr = (millis() / 1000) % 61;

	if (secNr != prevSec) {
		counts[secNr] = 0;
		prevSec = secNr;
	}

	counts[secNr]++;
}

WiFiClient wclient;
PubSubClient client(mqtt_server, 1883, callback, wclient);

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection... ");
		Serial.println(mqtt_server);

		char name[32];
		snprintf(name, sizeof name, "mqttcounter-%d", ESP.getChipId());

		// Attempt to connect
		if (client.connect(name)) {
			Serial.println(F("Connected"));
			break;
		}

		reboot();
	}

	client.subscribe("#");
}

void setup() {
	Serial.begin(115200);
 Serial.setDebugOutput(true);
 delay(1000);
	Serial.println(F("Init mqttcount"));

	setupWifi();

	client.setServer(mqtt_server, 1883);

	memset(counts, 0x00, sizeof counts);

	display.setBrightness(0x0f);
	uint8_t segments[] = { 1, 2, 4, 8 };
	display.setSegments(segments);

	Serial.println(F("Go!"));

	server = new ESP8266WebServer(80);
	server -> on("/", handleRoot);

	server -> begin();

	reconnect();
}

unsigned long int start = millis();

void loop() {
	unsigned long int now = millis();
	if (now - start >= 1000l) {
		uint16_t total = 0;

		int8_t secNr = (millis() / 1000) % 61;
		for (uint8_t i = 0; i < 60; i++)
			total += counts[(secNr + i + 1) % 61];

		Serial.println(total);
		display.showNumberDec(total);

		start = now;
	}

	client.loop();

	if (!client.connected())
		reconnect();
}
