#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "Core/Theme.h"
#include "Core/Display.h"
#include "Core/Storage.h"
#include "Core/Network.h"
#include "Core/WebHandlers.h"
#include "Data/Aircraft.h"
#include "Data/AircraftLock.h"
#include "Data/AircraftFetcher.h"
#include "Data/FetchTask.h"

M5Canvas Display::cv(&M5.Display);
int      Display::W = 0;
int      Display::H = 0;

Preferences Storage::_p;

WebServer Network::server(80);
DNSServer Network::dns;
bool      Network::apMode = false;

Aircraft AircraftFetcher::list[MAX_AC];
int      AircraftFetcher::count      = 0;
float    AircraftFetcher::obsLat     = 0;
float    AircraftFetcher::obsLon     = 0;
int      AircraftFetcher::obsRadius  = 100;
uint32_t AircraftFetcher::lastFetch  = 0;

SemaphoreHandle_t gAircraftMutex     = nullptr;

static void _api() {
	  FetchTask::lock();
	  float lat = AircraftFetcher::obsLat;
	  float lon = AircraftFetcher::obsLon;
	  int radius = AircraftFetcher::obsRadius;
	  int acCount = AircraftFetcher::count;
	  FetchTask::unlock();
	  String j = "{\"lat\":"    + String(lat,5)
	           + ",\"lon\":"    + String(lon,5)
	           + ",\"radius\":" + String(radius)
	           + ",\"ip\":\""   + Network::ip()   + "\""
	           + ",\"ssid\":\"" + Storage::getSSID() + "\""
	           + ",\"fetchMs\":" + String((uint32_t)FetchTask::lastFetchMs)
	           + ",\"acCount\":" + String(acCount)
	           + ",\"mode\":\""  + (FetchTask::focusIdx<0?"radar":"detail") + "\""
	           + "}";
  Network::server.sendHeader("Access-Control-Allow-Origin","*");
  Network::server.sendHeader("Cache-Control","no-store");
  Network::server.send(200,"application/json",j);
}

static void _setloc() {
  String lat=Network::server.arg("lat");
  String lon=Network::server.arg("lon");
  String r  =Network::server.arg("radius");
  if(lat.length()) Storage::setLat(lat.toFloat());
  if(lon.length()) Storage::setLon(lon.toFloat());
  if(r.length())   Storage::setRadius(r.toInt());
  FetchTask::setLocation(Storage::getLat(),Storage::getLon(),Storage::getRadius());
  FetchTask::triggerNow();
  Network::server.sendHeader("Access-Control-Allow-Origin","*");
  Network::server.send(200,"text/plain","ok");
}

static void _setwifi() {
  Storage::setSSID(Network::server.arg("ssid"));
  Storage::setPass(Network::server.arg("pass"));
  Network::server.send(200,"text/plain","ok");
  delay(1000);
  ESP.restart();
}

static void _save() {
  Storage::setSSID(Network::server.arg("ssid"));
  Storage::setPass(Network::server.arg("pass"));
  String lat=Network::server.arg("lat"),lon=Network::server.arg("lon"),r=Network::server.arg("radius");
  if(lat.length()) Storage::setLat(lat.toFloat());
  if(lon.length()) Storage::setLon(lon.toFloat());
  if(r.length())   Storage::setRadius(r.toInt());
  Network::server.send(200,"text/html","<html><body style='background:#050708;color:#ab03ff;font-family:monospace'><h2>VORIX</h2><p>Saved. Rebooting...</p></body></html>");
  delay(2000); ESP.restart();
}

static void _redirect() {
  Network::server.sendHeader("Location","http://192.168.4.1",true);
  Network::server.send(302,"text/plain","");
}

void webHandlers_registerSetup() {
  Network::server.on("/",     HTTP_GET,  [](){Network::server.send_P(200,"text/html",SETUP_HTML);});
  Network::server.on("/save", HTTP_POST, _save);
  Network::server.onNotFound(_redirect);
  Network::server.begin();
}

void webHandlers_registerNormal() {
  Network::server.on("/",       HTTP_GET,  [](){Network::server.send_P(200,"text/html",DASH_HTML);});
  Network::server.on("/api",    HTTP_GET,  _api);
  Network::server.on("/setloc", HTTP_POST, _setloc);
  Network::server.on("/setwifi",HTTP_POST, _setwifi);
  Network::server.onNotFound([](){Network::server.send(404,"text/plain","Not found");});
  Network::server.begin();
}

volatile bool     FetchTask::fetching     = false;
volatile uint32_t FetchTask::lastFetchMs  = 0;
volatile bool     FetchTask::newDataFlag  = false;
volatile int      FetchTask::focusIdx     = -1;
float             FetchTask::_lat         = 0;
float             FetchTask::_lon         = 0;
int               FetchTask::_radius      = 100;
TaskHandle_t      FetchTask::_handle      = nullptr;
volatile bool     FetchTask::_forceNow    = false;
volatile bool     FetchTask::_resetTimers = false;
