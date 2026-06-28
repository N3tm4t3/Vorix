#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
  #define CONFIG_ARDUINO_LOOP_STACK_SIZE 8192
#endif

#include <esp32-hal-cpu.h>
#include "Core/Theme.h"
#include "Core/Display.h"
#include "Core/Storage.h"
#include "Core/Network.h"
#include "Core/BootScreen.h"
#include "Core/WebHandlers.h"
#include "Data/Aircraft.h"
#include "Data/AircraftFetcher.h"
#include "Data/FetchTask.h"
#include "UI/UI.h"

UI gUI;

constexpr uint32_t RENDER_MS   = 67;
constexpr uint32_t LONG_MS     = 1500;

uint32_t gLastRender = 0;

struct LongBtn {
  uint32_t downAt  = 0;
  bool     held    = false;
  bool     fired   = false;
};
LongBtn gBtnA, gBtnB, gBtnC;

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  setCpuFrequencyMhz(160);
  M5.Speaker.setVolume(160);

  Display::begin();
  Storage::begin();
  Display::setBright((uint8_t)Storage::getBright());

  Boot::logo();
  for (int f : {700,1000,1400,1900}) { M5.Speaker.tone(f,60); delay(70); }
  delay(600);

  String ssid = Storage::getSSID();

  if (ssid.length() == 0) {
    Network::startAP();
    webHandlers_registerSetup();
    Boot::setup_();
    while (true) { Network::loop(); delay(5); }
  }

  Boot::connecting(ssid);
  bool ok = Network::connectSTA(ssid, Storage::getPass(),
    [](int p){ Boot::progress(p); });

  if (!ok) {
    Boot::error("WiFi failed");
    uint32_t t = millis();
    while (millis()-t < 8000) {
      M5.update();
      if (M5.BtnA.pressedFor(2000)) {
        Storage::clearWiFi(); delay(200); ESP.restart();
      }
      delay(40);
    }

  } else {
    Boot::connected(Network::ip());
    webHandlers_registerNormal();
  }

  if (ok) {
    FetchTask::begin(Storage::getLat(), Storage::getLon(),
                     Storage::getRadius());
  }

  gUI.screen = Screen::RADAR;
  gUI.dirty  = true;
}

void loop() {
  M5.update();
  uint32_t now = millis();

  Network::loop();

  if (M5.BtnA.wasPressed()) {
    gBtnA.downAt = now; gBtnA.held = true; gBtnA.fired = false;
  }
  if (gBtnA.held) {
    if (M5.BtnA.isPressed() && !gBtnA.fired && now-gBtnA.downAt >= LONG_MS) {
      gBtnA.fired = true; gBtnA.held = false;
      gUI.onALong();
    }
    if (M5.BtnA.wasReleased()) {
      gBtnA.held = false;
      if (!gBtnA.fired) gUI.onA();
    }
  }

  if (M5.BtnB.wasPressed()) {
    gBtnB.downAt = now; gBtnB.held = true; gBtnB.fired = false;
  }
  if (gBtnB.held) {
    if (M5.BtnB.isPressed() && !gBtnB.fired && now-gBtnB.downAt >= LONG_MS) {
      gBtnB.fired = true; gBtnB.held = false;
      gUI.onBLong();
    }
    if (M5.BtnB.wasReleased()) {
      gBtnB.held = false;
      if (!gBtnB.fired) gUI.onB();
    }
  }

  if (M5.BtnPWR.wasPressed()) {
    gBtnC.downAt = now; gBtnC.held = true; gBtnC.fired = false;
  }
  if (gBtnC.held) {
    if (M5.BtnPWR.isPressed() && !gBtnC.fired && now-gBtnC.downAt >= LONG_MS) {
      gBtnC.fired = true; gBtnC.held = false;
      gUI.onCLong();
    }
    if (M5.BtnPWR.wasReleased()) {
      gBtnC.held = false;
      if (!gBtnC.fired) gUI.onC();
    }
  }

  if (FetchTask::newDataFlag) {
    FetchTask::newDataFlag = false;
    gUI.markDirty();

    bool hasEmg = false;
    FetchTask::lock();
    for (int i = 0; i < AircraftFetcher::count; i++) {
      if (AircraftFetcher::list[i].emg()) { hasEmg = true; break; }
    }
    FetchTask::unlock();
    if (hasEmg) {
      for (int b=0;b<3;b++){M5.Speaker.tone(2600,100);delay(120);M5.Speaker.tone(700,100);delay(120);}
    }
  }

  if (now - gLastRender >= RENDER_MS) {
    gLastRender = now;
    gUI.tick(now);
  }
}
