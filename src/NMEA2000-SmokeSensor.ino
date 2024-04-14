/*
 Name:		NMEA2000_SmokeSensor.ino
 Created:	14.04.2024 12:26:36
 Author:	andy
*/


#include "neotimer.h"

#define SmokeSensorPin A0
#define FalmeSensorPin A3

uint32_t smokeSensorTreashold = 400;
Neotimer smokeSensorTimer = Neotimer(1000);

uint32_t flameSensorTreashold = 400;
Neotimer flameSensorTimer = Neotimer(1000);


// the setup function runs once when you press reset or power the board
void setup() {
	uint8_t chipid[6];
	uint32_t id = 0;
	int i = 0;

	// Generate unique number from chip id
	esp_efuse_mac_get_default(chipid);
	for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

	Serial.begin(115200);

	// wait for serial port to open on native usb devices
	while (!Serial) {
		delay(1);
	}

	pinMode(SmokeSensorPin, INPUT);
	pinMode(FalmeSensorPin, INPUT);
}

// the loop function runs over and over again until power down or reset
void loop() {
	if (smokeSensorTimer.repeat()) {
		uint32_t smokeSensorValue = analogRead(SmokeSensorPin);

		Serial.printf("SmokeSensorValue=%d\n", smokeSensorValue);

		if (smokeSensorValue > smokeSensorTreashold) {
			Serial.println("Smoke detected!");
		}
	}

	if (flameSensorTimer.repeat()) {
		uint32_t flameSensorValue = analogRead(FalmeSensorPin);

		Serial.printf("FlameSensorValue=%d\n", flameSensorValue);

		if (flameSensorValue > flameSensorTreashold) {
			Serial.println("Flame detected!");
		}
	}
}
