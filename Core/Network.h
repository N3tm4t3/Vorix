#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

class Network {
public:
  static WebServer server;
  static DNSServer dns;
  static bool      apMode;

  static bool connectSTA(const String& ssid, const String& pass,
                         void(*prog)(int) = nullptr) {
    apMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(250); yield();
      if (prog) prog(i * 100 / 40);
    }
    return WiFi.status() == WL_CONNECTED;
  }

  static void startAP(const char* name = "VORIX-SETUP") {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(name);
    dns.start(53, "*", WiFi.softAPIP());
  }

  static bool ok()    { return WiFi.status() == WL_CONNECTED; }
  static String ip()  { return WiFi.localIP().toString(); }
  static int rssi()   { return WiFi.RSSI(); }

  static void loop() {
    server.handleClient();
    if (apMode) dns.processNextRequest();
  }
};
