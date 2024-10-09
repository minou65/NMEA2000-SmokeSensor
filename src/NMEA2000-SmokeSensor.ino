// use the following Pins

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to D5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to D4

#include <esp_task_wdt.h>
#include <N2kMessages.h>
#include <NMEA2000_CAN.h>

#include "common.h"
#include "webhandling.h"
#include "neotimer.h"
#include "version.h"

char Version[] = VERSION_STR; // Manufacturer's Software version code

#define WDT_TIMEOUT 5

bool debugMode = true;

Neotimer WDtimer = Neotimer((WDT_TIMEOUT + 1) * 1000);

/*
Since the ADC2 module is also used by the Wi - Fi, only one of them could get the preemption when using together, 
which means the adc2_get_raw() may get blocked until Wi - Fi stops, and vice versa.

That means you can't use the ADC on any of the ADC2 channels while WiFi is on: 
	GPIO4, GPIO0, GPIO2, GPIO15, GPIO13, GPIO12, GPIO14, GPIO27, GPIO25 and GPIO26.

But you can use ADC1, which uses pins 
	GPIO36, GPIO37, GPIO38, GPIO39, GPIO32, GPIO33, GPIO34 and GPIO35.
*/

const uint8_t SensorPins[] = { GPIO_NUM_32, GPIO_NUM_33 };

uint64_t DeviceId1 = 0;
uint64_t DeviceId2 = 0;

Neotimer WriteOutputTimer(5000);

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
	WriteOutputTimer.start();

	Sensor* _sensor = &Sensor1;
	while (_sensor != nullptr) {
		_sensor->AlarmScheduler.UpdateNextTime();
		_sensor->TextAlarmScheduler.UpdateNextTime();
		_sensor = (Sensor*)_sensor->getNext();
	}
}

void CheckN2kSourceAddressChange() {

	if (NMEA2000.GetN2kSource() != gN2KSource) {
		gN2KSource = NMEA2000.GetN2kSource();
		gSaveParams = true;
	}
}

void setup() {
	
	Serial.begin(115200);
	while (!Serial) {
		delay(1);
	}
	Serial.println("NMEA2000-SmokeSensor v" + String(Version) + " started");

	InitDeviceId();
	wifiinit();

	// Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
	NMEA2000.SetN2kCANMsgBufSize(8);
	NMEA2000.SetN2kCANReceiveFrameBufSize(250);
	NMEA2000.SetN2kCANSendFrameBufSize(250);

	NMEA2000.SetOnOpen(OnN2kOpen);

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
	WriteOutputTimer.start();


	esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
	esp_task_wdt_add(NULL); //add current thread to WDT watch

	WDtimer.start();
}

void InitDeviceId() {
	uint8_t _chipid[6];
	int _i = 0;

	// Generate unique numbers from chip id
	esp_efuse_mac_get_default(_chipid);

	for (int _i = 0; _i < 6; _i++) {
		DeviceId1 += (_chipid[_i] << (7 * _i));
		DeviceId2 += (_chipid[_i] << (8 * _i)); // 8*_i statt 7*_i
	}
}

void UpdateAlertSystem() {
	if (NMEA2000.IsOpen()) InitAlertsystem();
}

void InitAlertsystem() {
	Sensor* _sensor = &Sensor1;
	uint8_t _index = 0;
	while (_sensor != nullptr) {
		_sensor->Alert.SetAlertSystem(_index, gN2KInstance + _index, NMEA2000.GetN2kSource(), N2kts_AlertLanguageEnglishUS, _sensor->descriptionValue, _sensor->locationValue);

		_sensor->Alert.SetAlertDataSource(gN2KInstance + _index, 0, NMEA2000.GetN2kSource());
		_sensor->Alert.SetAlertThreshold(t2kNAlertThresholdMethod(_sensor->GetThresholdMethod()), 0, _sensor->GetThresholdValue());

		_index++;
		_sensor = (Sensor*)_sensor->getNext();
	}
}

void SendAlarm() {
	tN2kMsg _N2kMsg;

	Sensor* _sensor = &Sensor1;
	while (_sensor != nullptr) {
		if (_sensor->AlarmScheduler.IsTime()) {
			_sensor->AlarmScheduler.UpdateNextTime();
			_sensor->Alert.SetN2kAlert(_N2kMsg);
			NMEA2000.SendMsg(_N2kMsg);
		}
		_sensor = (Sensor*)_sensor->getNext();
	}
}

void SendAlarmText() {
	tN2kMsg _N2kMsg;

	Sensor* _sensor = &Sensor1;
	while (_sensor != nullptr) {
		if (_sensor->TextAlarmScheduler.IsTime()) {
			_sensor->TextAlarmScheduler.UpdateNextTime();

			_sensor->Alert.SetN2kAlertText(_N2kMsg);
			NMEA2000.SendMsg(_N2kMsg);
		}
		_sensor = (Sensor*)_sensor->getNext();
	}
}

void loop() {
	Sensor* _sensor = &Sensor1;
	uint8_t _i = 0;

	if (WriteOutputTimer.done()) {
		while (_sensor != nullptr) {
			_sensor->SetSensorValue(analogRead(SensorPins[_i]));

			DEBUG_PRINTF("Sensor%d value = %s; threshold = %d\n", _i, String(_sensor->GetSensorValue(), 0), _sensor->GetThresholdValue());
			WebSerial.printf("Sensor%d value = %s; threshold = %d\n", _i, String(_sensor->GetSensorValue(), 0), _sensor->GetThresholdValue());
			if (_sensor->Alert.isAlert()) {
				DEBUG_PRINTLN(_sensor->descriptionValue);
				WebSerial.println(_sensor->descriptionValue);
			}
			_i++;
			_sensor = (Sensor*)_sensor->getNext();
		}
		WriteOutputTimer.start();
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

	if (WDtimer.repeat()) {
		esp_task_wdt_reset();
	}
}
