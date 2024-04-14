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

// -- Method declarations.
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
void wifiConnected();

bool gParamsChanged = true;

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

NMEAConfig Config = NMEAConfig();

void wifiinit() {

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
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");

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
}

void configSaved() {
    convertParams();
    gParamsChanged = true;
}

void handleRoot() {
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

}