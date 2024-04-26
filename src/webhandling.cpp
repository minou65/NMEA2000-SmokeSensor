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
#include <iostream>
#include <string.h>

#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <IotWebConfESP32HTTPUpdateServer.h>
#include "common.h"
#include "webhandling.h"
#include "favicon.h"
#include "IotWebRoot.h"

#define HTML_Start_Doc "<!DOCTYPE html>\
    <html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\
    <title>{v}</title>\
";

#define HTML_Start_Body "</head><body>";
#define HTML_Start_Fieldset "<fieldset align=left style=\"border: 1px solid\">";
#define HTML_Start_Table "<table border=0 align=center>";
#define HTML_End_Table "</table>";
#define HTML_End_Fieldset "</fieldset>";
#define HTML_End_Body "</body>";
#define HTML_End_Doc "</html>";
#define HTML_Fieldset_Legend "<legend>{l}</legend>"
#define HTML_Table_Row "<tr><td>{n}</td><td>{v}</td></tr>";
#define HTML_JAVA_Script \
"<script>\n \
    // get intial data straight away \n \
    requestData(); \n \
\n \
    // request data updates every milliseconds\n \
    setInterval(requestData, 5000);\n \
\n \
    function requestData() {\n \
		var xhttp = new XMLHttpRequest();\n \
		xhttp.onreadystatechange = function() {\n \
			if (this.readyState == 4 && this.status == 200) {\n \
				var json = JSON.parse(this.responseText);\n \
				updateData(json);\n \
			}\n \
		};\n \
        xhttp.open('GET', 'data', true);\n \
		xhttp.send();\n \
	}\n \
\n \
    function updateData(jsonData) { \n \
        document.getElementById('sensor1').innerHTML = jsonData.sensor1;\n \
		document.getElementById('sensor2').innerHTML = jsonData.sensor2;\n \
	}\n \
</script>\
";

// -- Method declarations.
void handleData();
void handleRoot();
void handleFavIcon();
void convertParams();
extern void UpdateAlertSystem();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

NMEAConfig Config = NMEAConfig();
GasSensor Sensor1 = GasSensor("Sensor1", "Sensor 1");
GasSensor Sensor2 = GasSensor("Sensor2", "Sensor 2");

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

    // -- Define how to handle updateServer calls.
    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    // -- Initializing the configuration.
    iotWebConf.init();

    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/favicon.ico", [] { handleFavIcon(); });
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.on("/data", HTTP_GET, []() { handleData(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

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

void handleFavIcon() {
    server.send_P(200, "image/x-icon", favicon_bits, sizeof(favicon_bits));
}

void handleData() {
    String _repsone = "{";
    GasSensor* _sensor = &Sensor1;
    uint8_t _i = 1;
    while (_sensor != nullptr) {
        _repsone += "\"sensor" + String(_i) + "\":\"" + _sensor->GetSensorValue() + "\"";
        _sensor = (GasSensor*)_sensor->getNext();
        _i++;
        if (_sensor != nullptr) {
			_repsone += ",";
		}
    }
    _repsone += "}";
	server.send(200, "text/plain", _repsone);
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
		_s += F("   document.getElementById('sensor1').innerHTML = jsonData.sensor1;\n");
		_s += F("   document.getElementById('sensor2').innerHTML = jsonData.sensor2;\n");
		_s += F("}\n");
        _s.replace("{millisecond}", "5000");
		return _s;
    }
};

void handleRoot() {
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()){
        // -- Captive portal request were already served.
        return;
    }

    MyHtmlRootFormatProvider rootFormatProvider;

    String _response = "";
    _response += rootFormatProvider.getHtmlHead(iotWebConf.getThingName());
    _response += rootFormatProvider.getHtmlStyle();
    _response += rootFormatProvider.getHtmlHeadEnd();
    _response += rootFormatProvider.getHtmlScript();

    _response += rootFormatProvider.getHtmlTable();
    _response += rootFormatProvider.getHtmlTableRow() + rootFormatProvider.getHtmlTableCol();

    _response += rootFormatProvider.getHtmlFieldset("Sensors");
    _response += rootFormatProvider.getHtmlTable();
    GasSensor* _sensor = &Sensor1;
    uint8_t _i = 1;
    while (_sensor != nullptr) {
        _response += rootFormatProvider.getHtmlTableRowSpan(_sensor->locationValue, "0", "sensor" + String(_i));
        _sensor = (GasSensor*)_sensor->getNext();
        _i++;
    }
    _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.getHtmlFieldset("Network");
    _response += rootFormatProvider.getHtmlTable();
    _response += rootFormatProvider.getHtmlTableRowText("MAC Address", WiFi.macAddress());
    _response += rootFormatProvider.getHtmlTableRowText("IP Address", WiFi.localIP().toString().c_str());
    _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.addNewLine(2);

    _response += rootFormatProvider.getHtmlTable();
    _response += rootFormatProvider.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.");
    _response += rootFormatProvider.getHtmlTableRowText(rootFormatProvider.getHtmlVersion(Version));
    _response += rootFormatProvider.getHtmlTableEnd();

    _response += rootFormatProvider.getHtmlTableColEnd() + rootFormatProvider.getHtmlTableRowEnd();
    _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlEnd();

    server.send(200, "text/html", _response);
}