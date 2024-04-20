// use the following Pins

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to D5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to D4

#include <N2kMessages.h>
#include <NMEA2000_CAN.h>

#include "common.h"
#include "webhandling.h"
#include "neotimer.h"

char Version[] = "0.0.0.1 (2024-04-14)"; // Manufacturer's Software version code

#define SmokeSensorPin A0
#define FalmeSensorPin A3

uint32_t smokeSensorTreashold = 400;
uint32_t flameSensorTreashold = 400;

uint64_t DeviceId1 = 0;
uint64_t DeviceId2 = 0;

Neotimer smokeSensorTimer(500);
Neotimer flameSensorTimer(500);

uint8_t gN2KSource = 22;
uint8_t gN2KInstance = 1;
uint8_t gN2KSID = 255;

bool gSaveParams = false;

// List here messages your device will transmit.
const unsigned long AlarmDeviceDeviceMessages[] PROGMEM = {
	126983L, // Alert
	126984L, // Alert response
	126985L, // Alert text
	0
};

void OnN2kOpen() {
	// Start schedulers now.
	smokeSensorTimer.start();
	flameSensorTimer.start();

}

void CheckN2kSourceAddressChange() {

	if (NMEA2000.GetN2kSource() != gN2KSource) {
		gN2KSource = NMEA2000.GetN2kSource();
		gSaveParams = true;
	}
}

// the setup function runs once when you press reset or power the board
void setup() {
	
	Serial.begin(115200);

	// wait for serial port to open on native usb devices
	while (!Serial) {
		delay(1);
	}

	InitDeviceId();
	wifiinit();

	pinMode(SmokeSensorPin, INPUT);
	pinMode(FalmeSensorPin, INPUT);

	NMEA2000.SetOnOpen(OnN2kOpen);

	// Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
	NMEA2000.SetN2kCANMsgBufSize(8);
	NMEA2000.SetN2kCANReceiveFrameBufSize(150);
	NMEA2000.SetN2kCANSendFrameBufSize(150);

	// Set Product information
	NMEA2000.SetProductInformation(
		"1", // Manufacturer's Model serial code
		100, // Manufacturer's product code
		"SmokeSensor",  // Manufacturer's Model ID
		Version,  // Manufacturer's Software version code
		Version // Manufacturer's Model version
	);

	NMEA2000.SetDeviceInformation(
		DeviceId1, // Unique number. Use e.g. Serial number.
		110, // Device function=Alarm Enunciator. See DEVICE_FUNCTION (0 - 255) https://canboat.github.io/canboat/canboat.html#main
		20, // Device class=Safety systems. DEVICE_CLASS (0 - 127) https://canboat.github.io/canboat/canboat.html#main
		2040 // Just choosen free from code list on MANUFACTURER_CODE (0 - 2047) https://canboat.github.io/canboat/canboat.html#main
	);

#ifdef DEBUG_NMEA_MSG
	Serial.begin(115200);
	NMEA2000.SetForwardStream(&Serial);

#ifdef DEBUG_NMEA_MSG_ASCII
	NMEA2000.SetForwardType(tNMEA2000::fwdt_Text)
#endif // DEBUG_NMEA_MSG_ASCII

#ifdef  DEBUG_NMEA_Actisense
		NMEA2000.SetDebugMode(tNMEA2000::dm_Actisense);
#endif //  DEBUG_NMEA_Actisense

#else
	NMEA2000.EnableForward(false); // Disable all msg forwarding to USB (=Serial)

#endif // DEBUG_NMEA_MSG

	// If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
	NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly);

	// If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
	NMEA2000.SetN2kSource(gN2KSource);

	// Here we tell library, which PGNs we transmit
	NMEA2000.ExtendTransmitMessages(AlarmDeviceDeviceMessages);

	NMEA2000.Open();

	InitAlertsystem();
}

void InitDeviceId() {
	uint8_t chipid[6];
	int i = 0;

	// Generate unique numbers from chip id
	esp_efuse_mac_get_default(chipid);

	for (int i = 0; i < 6; i++) {
		DeviceId1 += (chipid[i] << (7 * i));
		DeviceId2 += (chipid[i] << (8 * i)); // 8*i statt 7*i
	}
}

void UpdateAlertSystem() {
	if (NMEA2000.IsOpen()) InitAlertsystem();
}

void InitAlertsystem() {
	GasSensor* _sensor = &Sensor1;
	uint8_t _index = 0;
	while (_sensor != nullptr) {
		_sensor->Alert.SetAlertSystem(_index, gN2KInstance + _index, NMEA2000.GetN2kSource(), N2kts_AlertLanguageEnglishUS, _sensor->descriptionValue, _sensor->locationValue);

		_sensor->Alert.SetAlertDataSource(gN2KInstance + _index, 0, NMEA2000.GetN2kSource());
		_sensor->Alert.SetAlertThreshold(t2kNAlertThresholdMethod(_sensor->GetThresholdMethod()), 0, _sensor->GetThresholdValue());

		_index++;
		_sensor = (GasSensor*)_sensor->getNext();
	}
}

void SendAlarm() {
	tN2kMsg N2kMsg;

	GasSensor* _sensor = &Sensor1;
	while (_sensor != nullptr) {
		if (_sensor->AlarmScheduler.IsTime()) {
			_sensor->AlarmScheduler.UpdateNextTime();
			_sensor->Alert.SetN2kAlert(N2kMsg);
			NMEA2000.SendMsg(N2kMsg);
		}
		_sensor = (GasSensor*)_sensor->getNext();
	}
}

void SendAlarmText() {
	tN2kMsg N2kMsg;

	GasSensor* _sensor = &Sensor1;
	while (_sensor != nullptr) {
		if (_sensor->TextAlarmScheduler.IsTime()) {
			_sensor->TextAlarmScheduler.UpdateNextTime();

			_sensor->Alert.SetN2kAlertText(N2kMsg);
			NMEA2000.SendMsg(N2kMsg);
		}
		_sensor = (GasSensor*)_sensor->getNext();
	}
}

// the loop function runs over and over again until power down or reset
void loop() {
	if (smokeSensorTimer.repeat()) {
		uint32_t smokeSensorValue = analogRead(SmokeSensorPin);

		Serial.printf("SmokeSensorValue = %d\n", smokeSensorValue);
		Sensor1.SetSensorValue(smokeSensorValue);

		if (smokeSensorValue > smokeSensorTreashold) {
			Serial.println("Smoke detected!");
		}
	}

	if (flameSensorTimer.repeat()) {
		uint32_t flameSensorValue = analogRead(FalmeSensorPin);

		Serial.printf("FlameSensorValue = %d\n", flameSensorValue);
		Sensor2.SetSensorValue(flameSensorValue);

		if (flameSensorValue > flameSensorTreashold) {
			Serial.println("Flame detected!");
		}
	}

	SendAlarm();
	SendAlarmText();

	NMEA2000.ParseMessages();
	wifiloop();

	CheckN2kSourceAddressChange();

	// Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
	if (Serial.available()) {
		Serial.read();
	}
}
