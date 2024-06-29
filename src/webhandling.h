// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <WebSerial.h>
#include <N2kTimer.h>
#include "N2kAlerts.h"

#define STRING_LEN 60
#define NUMBER_LEN 5


// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "NMEA2000-SmokeSensor";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  GPIO_NUM_13

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif

void wifiinit();
void wifiloop();

extern IotWebConf iotWebConf;

class NMEAConfig : public iotwebconf::ParameterGroup {
public:
    NMEAConfig() : ParameterGroup("nmeaconfig", "NMEA configuration") {
        snprintf(instanceID, STRING_LEN, "%s-instance", this->getId());
        snprintf(sidID, STRING_LEN, "%s-sid", this->getId());
        snprintf(sourceID, STRING_LEN, "%s-source", this->getId());

        this->addItem(&this->InstanceParam);
        this->addItem(&this->SIDParam);

        iotWebConf.addHiddenParameter(&SourceParam);
    }

    uint8_t Instance() { return atoi(InstanceValue); };
    uint8_t SID() { return atoi(SIDValue); };
    uint8_t Source() { return atoi(SourceValue); };

    void SetSource(uint8_t source_) {
        String s;
        s = (String)source_;
        strncpy(SourceParam.valueBuffer, s.c_str(), NUMBER_LEN);
    }

private:
    iotwebconf::NumberParameter InstanceParam = iotwebconf::NumberParameter("Instance", instanceID, InstanceValue, NUMBER_LEN, "1", "1..255", "min='1' max='254' step='1'");
    iotwebconf::NumberParameter SIDParam = iotwebconf::NumberParameter("SID", sidID, SIDValue, NUMBER_LEN, "255", "1..255", "min='1' max='255' step='1'");
    iotwebconf::NumberParameter SourceParam = iotwebconf::NumberParameter("Source", sourceID, SourceValue, NUMBER_LEN, "22", nullptr, nullptr);

    char InstanceValue[NUMBER_LEN];
    char SIDValue[NUMBER_LEN];
    char SourceValue[NUMBER_LEN];


    char instanceID[STRING_LEN];
    char sidID[STRING_LEN];
    char sourceID[STRING_LEN];
};

static char ThresholdMethodValues[][STRING_LEN] = {
    "0",
    "1",
    "2"
};

static char ThresholdMethodNames[][STRING_LEN] = {
    "equal",
    "lower than",
    "greater than"
};

static char SensorTypeValues[][STRING_LEN] = {
	"0",
	"1"
};

static char SensorTypeNames[][STRING_LEN] = {
	"Gas / Smoke",
	"Flame"
};

class Sensor : public iotwebconf::ParameterGroup {
public:

    Sensor(const char* id, const char* lable) : ParameterGroup(id, lable) {

        snprintf(SensorTypeID, STRING_LEN, "%s-sensortype", this->getId());
        snprintf(thresholdId, STRING_LEN, "%s-threshold", this->getId());
        snprintf(methodId, STRING_LEN, "%s-method", this->getId());
        snprintf(descriptionId, STRING_LEN, "%s-description", this->getId());
        snprintf(locationId, STRING_LEN, "%s-location", this->getId());

        this->addItem(&this->SensorTypeParam);
        this->addItem(&this->ThresholdParam);
        this->addItem(&this->MethodParam);
        this->addItem(&this->LocationParam);
        this->addItem(&this->DescriptionParam);
    }

    iotwebconf::SelectParameter SensorTypeParam = iotwebconf::SelectParameter("SensorType", SensorTypeID, SensorTypeValue, STRING_LEN,
        (char*)SensorTypeValues, (char*)SensorTypeNames, sizeof(SensorTypeValues) / STRING_LEN, STRING_LEN);

    iotwebconf::NumberParameter ThresholdParam = iotwebconf::NumberParameter("Threshold", thresholdId, thresholdValue, NUMBER_LEN, "400", "100..3300", "min='100' max='3300' step='1'");

    iotwebconf::SelectParameter MethodParam = iotwebconf::SelectParameter("Method", methodId, methodValue, STRING_LEN,
        (char*)ThresholdMethodValues, (char*)ThresholdMethodNames, sizeof(ThresholdMethodValues) / STRING_LEN, STRING_LEN, "2");

    iotwebconf::TextParameter LocationParam = iotwebconf::TextParameter("Location", locationId, locationValue, STRING_LEN, "Location");
    iotwebconf::TextParameter DescriptionParam = iotwebconf::TextParameter("Alert message", descriptionId, descriptionValue, STRING_LEN, "Alert");

    void SetSensorValue(const double v) { 
        value = v; 
		Alert.TestAlertThreshold(v);
    };
    double GetSensorValue() const { return value; };
    uint8_t GetThresholdMethod() const { return atoi(methodValue); };
    uint32_t GetThresholdValue() const { return atoi(thresholdValue); };
    char* GetSensorType() const { return SensorTypeNames[atoi(SensorTypeValue)]; };


    void setNext(Sensor* nextGroup) { this->nextGroup = nextGroup; nextGroup->prevGroup = this; };
    Sensor* getNext() { return this->nextGroup; };

    tN2kSyncScheduler AlarmScheduler = tN2kSyncScheduler(false, 500, 100);
    tN2kSyncScheduler TextAlarmScheduler = tN2kSyncScheduler(false, 10000, 2000);
    tN2kAlert Alert = tN2kAlert(N2kts_AlertTypeCaution, N2kts_AlertCategoryTechnical, 100, N2kts_AlertTriggerAuto, 100, N2kts_AlertNo, N2kts_AlertYes);

    char SensorTypeValue[STRING_LEN];
    char thresholdValue[NUMBER_LEN];
    char methodValue[STRING_LEN];
    char descriptionValue[STRING_LEN];
    char locationValue[STRING_LEN];

protected:
    Sensor* prevGroup = nullptr;
    Sensor* nextGroup = nullptr;

private:

    char SensorTypeID[STRING_LEN];
	char thresholdId[STRING_LEN];
	char methodId[STRING_LEN];
	char descriptionId[STRING_LEN];
    char locationId[STRING_LEN];


	double value = 0;
};

extern Sensor Sensor1;
extern Sensor Sensor2;

#endif

