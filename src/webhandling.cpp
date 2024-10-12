#define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>
#if ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>      
#endif

#include <time.h>
//needed for library

#include <DNSServer.h>
#include <IotWebConf.h>
#include <IotWebConfAsyncClass.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRoot.h>

#include "common.h"
#include "webhandling.h"
#include "favicon.h"
#include "IotWebRoot.h"

// -- Method declarations.
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void convertParams();
extern void UpdateAlertSystem();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;

IotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

NMEAConfig Config = NMEAConfig();
Sensor Sensor1 = Sensor("Sensor1", "Sensor 1");
Sensor Sensor2 = Sensor("Sensor2", "Sensor 2");

class CustomHtmlFormatProvider : public iotwebconf::OptionalGroupHtmlFormatProvider {
protected:
    virtual String getFormEnd() {
        String _s = OptionalGroupHtmlFormatProvider::getFormEnd();
        _s += F("</br>Return to <a href='/'>home page</a>.");
        return _s;
    }
};
CustomHtmlFormatProvider customHtmlFormatProvider;

void wifiinit() {
    Serial.println();
    Serial.println("starting up...");

    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    Sensor1.setNext(&Sensor2);

    iotWebConf.addParameterGroup(&Config);
    iotWebConf.addParameterGroup(&Sensor1);
    iotWebConf.addParameterGroup(&Sensor2);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    // -- Initializing the configuration.
    iotWebConf.init();

    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper(request);
        iotWebConf.handleConfig(&asyncWebRequestWrapper);
        }
    );
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
        request->send(response);
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });

    server.onNotFound([](AsyncWebServerRequest* request) {
        AsyncWebRequestWrapper asyncWebRequestWrapper(request);
        iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

    WebSerial.begin(&server, "/webserial");

    Serial.println("ready.");

}

void wifiloop() {
  iotWebConf.doLoop();
  ArduinoOTA.handle();

  if (gSaveParams) {
      Serial.println(F("Parameters are changed,save them"));

      Config.SetSource(gN2KSource);

      iotWebConf.saveConfig();
      gSaveParams = false;
  }

  if (AsyncUpdater.isFinished()) {
      Serial.println(F("Firmware update finished"));
      delay(1000);
      ESP.restart();
  }
}

void wifiConnected() {
    ArduinoOTA.begin();
}

void convertParams() {
    gN2KSource = Config.Source();

    UpdateAlertSystem();
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}

void handleData(AsyncWebServerRequest* request) {

	String json_ = "{";
	json_ += "\"rssi\":" + String(WiFi.RSSI());
	Sensor* sensor_ = &Sensor1;
	uint8_t i_ = 1;
	while (sensor_ != nullptr) {
		json_ += ",\"sensor" + String(i_) + "\":" + sensor_->GetSensorValue();
		sensor_ = (Sensor*)sensor_->getNext();
		i_++;
	}
	json_ += "}";
	request->send(200, "application/json", json_);
}

class MyHtmlRootFormatProvider : public HtmlRootFormatProvider {
protected:
    virtual String getStyleInner() {
		String _s = HtmlRootFormatProvider::getStyleInner();
		_s += F(".dot-grey{height: 12px; width: 12px; background-color: #bbb; border-radius: 50%; display: inline-block; }\n");
		_s += F(".dot-green{height: 12px; width: 12px; background-color: green; border-radius: 50%; display: inline-block; }\n");
		return _s;
	}

    virtual String getScriptInner() { 
        String _s = HtmlRootFormatProvider::getScriptInner();
		_s += F("function updateData(jsonData) {\n");
        _s += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
		_s += F("   document.getElementById('sensor1').innerHTML = jsonData.sensor1;\n");
		_s += F("   document.getElementById('sensor2').innerHTML = jsonData.sensor2;\n");
		_s += F("}\n");
        _s.replace("{millisecond}", "5000");
		return _s;
    }
};

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    std::string content_;
    MyHtmlRootFormatProvider fp_;

    content_ += fp_.getHtmlHead(iotWebConf.getThingName()).c_str();
    content_ += fp_.getHtmlStyle().c_str();
    content_ += fp_.getHtmlHeadEnd().c_str();
    content_ += fp_.getHtmlScript().c_str();
    content_ += fp_.getHtmlTable().c_str();
    content_ += fp_.getHtmlTableRow().c_str();
    content_ += fp_.getHtmlTableCol().c_str();

	content_ += String(F("<fieldset align=left style=\"border: 1px solid\">\n")).c_str();
	content_ += String(F("<table border=\"0\" align=\"center\" width=\"100%\">\n")).c_str();
	content_ += String(F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">-100</span></td></tr>\n")).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Sensors").c_str();
	content_ += fp_.getHtmlTable().c_str();
	Sensor* _sensor = &Sensor1;
	uint8_t _i = 1;
    while (_sensor != nullptr) {
		content_ += fp_.getHtmlTableRowSpan(_sensor->locationValue + String(":"), "0", "sensor" + String(_i)).c_str();
		_sensor = (Sensor*)_sensor->getNext();
		_i++;
    }

	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Network").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowText("MAC Address", WiFi.macAddress()).c_str();
	content_ += fp_.getHtmlTableRowText("IP Address", WiFi.localIP().toString().c_str()).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.addNewLine(2).c_str();

	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.").c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'webserial'>sensor monitoring</a> page.").c_str();
	content_ += fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();

    content_ += fp_.getHtmlTableColEnd().c_str();
    content_ += fp_.getHtmlTableRowEnd().c_str();
    content_ += fp_.getHtmlTableEnd().c_str();
    content_ += fp_.getHtmlEnd().c_str();

    AsyncWebServerResponse* response = request->beginChunkedResponse("text/html", [content_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

        std::string chunk_ = "";
        size_t len_ = min(content_.length() - index, maxLen);
        if (len_ > 0) {
            chunk_ = content_.substr(index, len_);
            chunk_.copy((char*)buffer, chunk_.length());
        }
        if (index + len_ <= content_.length())
            return chunk_.length();
        else
            return 0;

        });
    response->addHeader("Server", "ESP Async Web Server");
    request->send(response);
}