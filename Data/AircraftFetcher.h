#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Aircraft.h"
#include "AircraftLock.h"
#include "../Core/Network.h"

class AircraftFetcher {
public:
  static Aircraft list[MAX_AC];
  static int      count;
  static float    obsLat, obsLon;
  static int      obsRadius;
  static uint32_t lastFetch;

	  static bool fetchNearbyLite(float lat, float lon, int radius) {
	    if (!Network::ok()) return false;
	    Aircraft* old = new(std::nothrow) Aircraft[MAX_AC];
	    if (!old) return false;
	    int oldCount = 0;
	    aircraftLock();
	    oldCount = count;
	    for (int i=0; i<oldCount; i++) old[i] = list[i];
	    aircraftUnlock();

	    aircraftLock();
	    obsLat=lat; obsLon=lon; obsRadius=radius;
	    aircraftUnlock();

    WiFiClientSecure cli; cli.setInsecure();
    HTTPClient http;
    http.begin(cli, "https://api.adsb.lol/v2/lat/"+String(lat,4)
                   +"/lon/"+String(lon,4)+"/dist/"+String(radius));
    http.setTimeout(8000);
	    if (http.GET()!=200) { http.end(); delete[] old; return false; }

    DynamicJsonDocument* doc = new(std::nothrow) DynamicJsonDocument(14336);
	    if (!doc) { http.end(); delete[] old; return false; }
    DeserializationError err = deserializeJson(*doc, http.getStream());
    http.end();
	    if (err) { delete doc; delete[] old; return false; }

    Aircraft* tmp = new(std::nothrow) Aircraft[MAX_AC];
	    if (!tmp) { delete doc; delete[] old; return false; }

	    int nc = 0;
	    bool seen[MAX_AC];
	    for (int i=0; i<MAX_AC; i++) seen[i] = false;
	    for (JsonObject ac : (*doc)["ac"].as<JsonArray>()) {
      if (nc>=MAX_AC || !ac["lat"] || !ac["lon"]) continue;
      String hex = ac["hex"].as<String>();

      int existing = -1;
	      for (int i=0; i<oldCount; i++) {
	        if (old[i].hex == hex) { existing = i; break; }
	      }

	      if (existing >= 0) {
	        tmp[nc] = old[existing];
	        tmp[nc].fadingOut = false;
	        seen[existing] = true;
      } else {
        tmp[nc]           = Aircraft();
        tmp[nc].hex       = hex;
        tmp[nc].isNew     = true;
        tmp[nc].fadeAlpha = 0;
      }

      _fillLite(tmp[nc], ac);
      _geo(tmp[nc]);
	      tmp[nc].startInterp();
	      nc++;
	    }
	    uint32_t nowMs = millis();
	    for (int i=0; i<oldCount && nc<MAX_AC; i++) {
	      if (seen[i] || old[i].fadeAlpha == 0) continue;
	      if (nowMs - old[i].lastSeen > 20000) continue;
	      old[i].fadingOut = true;
	      old[i].isNew = false;
	      tmp[nc++] = old[i];
	    }
	    delete doc;
	    _sort(tmp, nc);
	    aircraftLock();
	    obsLat = lat;
	    obsLon = lon;
	    obsRadius = radius;
	    count = nc;
	    lastFetch = millis();
	    for(int i=0; i<count; i++) list[i] = tmp[i];
	    aircraftUnlock();
	    delete[] tmp;
	    delete[] old;
	    return true;
	  }

	  static bool fetchNearbyBounded(float lat, float lon, int radius) {
    if (!Network::ok()) return false;
    Aircraft* old = new(std::nothrow) Aircraft[MAX_AC];
    Aircraft* tmp = new(std::nothrow) Aircraft[MAX_AC];
    if (!old || !tmp) { delete[] old; delete[] tmp; return false; }

    int oldCount = 0;
    aircraftLock();
    oldCount = count;
    for (int i=0; i<oldCount; i++) old[i] = list[i];
    obsLat=lat; obsLon=lon; obsRadius=radius;
    aircraftUnlock();

    int apiRadiusNm = _apiRadiusNm(radius);
    WiFiClientSecure cli; cli.setInsecure(); cli.setTimeout(8000);
    HTTPClient http;
    http.begin(cli, "https://api.adsb.lol/v2/lat/"+String(lat,4)
                   +"/lon/"+String(lon,4)+"/dist/"+String(apiRadiusNm));
    http.setTimeout(8000);
    int code = http.GET();
	    if (code != 200) {
      Serial.printf("[ADS-B] HTTP %d\n", code);
      http.end(); delete[] tmp; delete[] old; return false;
	    }

    // Stream the aircraft array one object at a time so large ADS-B responses do not exhaust heap.
	    Stream& rawBody = http.getStream();
	    BlockingStream body(rawBody, 8000);
    if (!_seekAircraftArray(body)) {
      Serial.println("[ADS-B] aircraft array not found");
      http.end(); delete[] tmp; delete[] old; return false;
    }

    StaticJsonDocument<512> filter;
    const char* keys[] = {
      "hex","flight","lat","lon","gs","speed","spd","tas","ias",
      "track","true_heading","mag_heading","alt_baro","alt_geom","altitude"
    };
    for (const char* key : keys) filter[key] = true;
    DynamicJsonDocument doc(2048);

    int nc = 0;
    bool parseFailed = false;
    bool seen[MAX_AC] = {};
    while (nc < MAX_AC) {
      bool arrayEnded = false;
      if (!_seekNextObject(body, arrayEnded)) {
        parseFailed = !arrayEnded;
        break;
      }
      doc.clear();
      DeserializationError err = deserializeJson(
        doc, body, DeserializationOption::Filter(filter));
      if (err) {
        Serial.printf("[ADS-B] JSON %s\n", err.c_str());
        parseFailed = true;
        break;
      }

      JsonObject ac = doc.as<JsonObject>();
      if (!ac["lat"] || !ac["lon"]) continue;
      String hex = ac["hex"].as<String>();
      if (!hex.length()) continue;

      int existing = -1;
      for (int i=0; i<oldCount; i++) {
        if (old[i].hex == hex) { existing = i; break; }
      }
      if (existing >= 0) {
        tmp[nc] = old[existing];
        tmp[nc].fadingOut = false;
        seen[existing] = true;
      } else {
        tmp[nc] = Aircraft();
        tmp[nc].hex = hex;
        tmp[nc].isNew = true;
        tmp[nc].fadeAlpha = 0;
      }

      _fillLite(tmp[nc], ac);
      _geo(tmp[nc]);
      tmp[nc].startInterp();
      nc++;
    }
    http.end();
    if (parseFailed) { delete[] tmp; delete[] old; return false; }

    uint32_t nowMs = millis();
    for (int i=0; i<oldCount && nc<MAX_AC; i++) {
      if (seen[i] || old[i].fadeAlpha == 0) continue;
      if (nowMs - old[i].lastSeen > 20000) continue;
      old[i].fadingOut = true;
      old[i].isNew = false;
      tmp[nc++] = old[i];
    }

    _sort(tmp, nc);
    aircraftLock();
    count = nc;
    lastFetch = millis();
    for(int i=0; i<count; i++) list[i] = tmp[i];
    aircraftUnlock();
    Serial.printf("[ADS-B] %d aircraft, %d km (%d NM request)\n",
                  nc, radius, apiRadiusNm);
    delete[] tmp;
    delete[] old;
    return true;
  }

  static bool fetchNearby(float lat, float lon, int radius) {
    if (!Network::ok()) return false;
    obsLat=lat; obsLon=lon; obsRadius=radius;

    WiFiClientSecure cli; cli.setInsecure();
    HTTPClient http;
    http.begin(cli, "https://api.adsb.lol/v2/lat/"+String(lat,4)
                   +"/lon/"+String(lon,4)+"/dist/"+String(radius));
    http.setTimeout(8000);
    if (http.GET()!=200) { http.end(); return false; }

    DynamicJsonDocument* doc = new(std::nothrow) DynamicJsonDocument(14336);
    if (!doc) { http.end(); return false; }
    DeserializationError err = deserializeJson(*doc, http.getStream());
    http.end();
    if (err) { delete doc; return false; }

    Aircraft* tmp = new(std::nothrow) Aircraft[MAX_AC];
    if (!tmp) { delete doc; return false; }

    int nc = 0;
    for (JsonObject ac : (*doc)["ac"].as<JsonArray>()) {
      if (nc>=MAX_AC || !ac["lat"] || !ac["lon"]) continue;
      tmp[nc] = Aircraft();
      tmp[nc].hex = ac["hex"].as<String>();
      for (int i=0;i<count;i++) {
        if (list[i].hex==tmp[nc].hex) {
          tmp[nc].routeLoaded  = list[i].routeLoaded;
          tmp[nc].from         = list[i].from;
          tmp[nc].to           = list[i].to;
          tmp[nc].airline      = list[i].airline;
          tmp[nc].flnum        = list[i].flnum;
          tmp[nc].trackLen     = list[i].trackLen;
          tmp[nc].lastTrackTs  = list[i].lastTrackTs;
          tmp[nc].fadeAlpha    = list[i].fadeAlpha;
          tmp[nc].isNew        = list[i].isNew;
          tmp[nc].prevLat      = list[i].prevLat;
          tmp[nc].prevLon      = list[i].prevLon;
          tmp[nc].prevBrg      = list[i].prevBrg;
          tmp[nc].prevHdg      = list[i].prevHdg;
          tmp[nc].targLat      = list[i].targLat;
          tmp[nc].targLon      = list[i].targLon;
          tmp[nc].interpT      = list[i].interpT;
          memcpy(tmp[nc].track, list[i].track, sizeof(TrackPt)*list[i].trackLen);
          break;
        }
      }
      _fill(tmp[nc], ac);
      _geo(tmp[nc]);
      tmp[nc].startInterp();
      tmp[nc].addTrack();
      nc++;
    }
    delete doc;
    _sort(tmp, nc);
    count=nc; lastFetch=millis();
    for(int i=0;i<count;i++) list[i]=tmp[i];
    delete[] tmp;
    return true;
  }

	  static void fetchDetail(int idx) {
	    if (idx<0||!Network::ok()) return;
	    String hex;
	    aircraftLock();
	    if (idx>=0 && idx<count) hex = list[idx].hex;
	    aircraftUnlock();
	    if (!hex.length()) return;
	    WiFiClientSecure cli; cli.setInsecure();
	    HTTPClient http;
	    http.begin(cli,"https://api.adsb.lol/v2/icao/"+hex);
	    http.setTimeout(5000);
	    if (http.GET()==200) {
	      DynamicJsonDocument* doc=new(std::nothrow) DynamicJsonDocument(5120);
	      if (doc) {
	        if (!deserializeJson(*doc,http.getStream())) {
	          JsonArray a=(*doc)["ac"];
	          if (a.size()>0) {
	            aircraftLock();
	            if (idx>=0 && idx<count && list[idx].hex == hex) {
	              _fill(list[idx],a[0]);
	              _geo(list[idx]);
	              list[idx].startInterp();
	              list[idx].addTrack();
	            }
	            aircraftUnlock();
	          }
	        }
	        delete doc;
	      }
    }
    http.end();
  }

	  static void fetchRoute(int idx) {
	    if (idx<0||!Network::ok()) return;
	    String cs, hex;
	    bool loaded = true;
	    aircraftLock();
	    if (idx>=0 && idx<count) {
	      cs = list[idx].flight;
	      hex = list[idx].hex;
	      loaded = list[idx].routeLoaded;
	    }
	    aircraftUnlock();
	    if (!hex.length() || loaded) return;
	    cs.trim(); cs.replace(" ","");
	    if (cs.length()<3) {
	      aircraftLock();
	      if (idx>=0 && idx<count && list[idx].hex == hex) list[idx].routeLoaded=true;
	      aircraftUnlock();
	      return;
	    }
	    WiFiClientSecure cli; cli.setInsecure();
	    HTTPClient http;
	    http.begin(cli,"https://api.adsbdb.com/v0/callsign/"+cs);
	    http.setTimeout(5000);
	    String flnum, airline, from, to, icao;
	    if (http.GET()==200) {
	      DynamicJsonDocument* doc=new(std::nothrow) DynamicJsonDocument(5120);
	      if (doc) {
	        if (!deserializeJson(*doc,http.getStream())) {
	          JsonObject fr=(*doc)["response"]["flightroute"];
	          flnum  =fr["callsign_iata"]|"";
	          airline=fr["airline"]["name"]|"";
	          from   =fr["origin"]["iata_code"]|"";
	          to     =fr["destination"]["iata_code"]|"";
	          icao   =fr["callsign_icao"]|"";
	        }
	        delete doc;
	      }
	    }
	    aircraftLock();
	    if (idx>=0 && idx<count && list[idx].hex == hex) {
	      list[idx].flnum  = flnum;
	      list[idx].airline= airline;
	      list[idx].from   = from;
	      list[idx].to     = to;
	      if(icao.length()) list[idx].flight=icao;
	      list[idx].routeLoaded=true;
	    }
	    aircraftUnlock();
	    http.end();
	  }

private:
  class BlockingStream : public Stream {
  public:
    BlockingStream(Stream& source, uint32_t timeoutMs)
      : _source(source), _timeoutMs(timeoutMs) {}

    int available() override { return _source.available(); }

    int read() override {
      uint32_t deadline = millis() + _timeoutMs;
      do {
        int c = _source.read();
        if (c >= 0) return c;
        delay(1);
      } while ((int32_t)(deadline - millis()) > 0);
      return -1;
    }

    int peek() override {
      uint32_t deadline = millis() + _timeoutMs;
      do {
        int c = _source.peek();
        if (c >= 0) return c;
        delay(1);
      } while ((int32_t)(deadline - millis()) > 0);
      return -1;
    }

    size_t write(uint8_t) override { return 0; }

  private:
    Stream& _source;
    uint32_t _timeoutMs;
  };

  static int _apiRadiusNm(int radiusKm) {
    return max(1, (int)ceilf(radiusKm / 1.852f));
  }

  static bool _seekAircraftArray(Stream& stream) {
    static const char token[] = "\"ac\"";
    int matched = 0;
    uint32_t deadline = millis() + 8000;
    while ((int32_t)(deadline - millis()) > 0) {
      int c = stream.read();
      if (c < 0) { delay(1); continue; }
      if (c == token[matched]) matched++;
      else matched = (c == token[0]) ? 1 : 0;
      if (matched == 4) {
        while ((int32_t)(deadline - millis()) > 0) {
          c = stream.read();
          if (c < 0) { delay(1); continue; }
          if (c == '[') return true;
        }
      }
    }
    return false;
  }

  static bool _seekNextObject(Stream& stream, bool& arrayEnded) {
    arrayEnded = false;
    uint32_t deadline = millis() + 8000;
    while ((int32_t)(deadline - millis()) > 0) {
      int c = stream.peek();
      if (c < 0) { delay(1); continue; }
      if (c == '{') return true;
      stream.read();
      if (c == ']') { arrayEnded = true; return false; }
    }
    return false;
  }

  static void _fillLite(Aircraft& a, JsonObject ac) {
    String fl=ac["flight"]|""; fl.trim(); if(fl.length()) a.flight=fl;
    if(ac.containsKey("lat")) a.lat=ac["lat"].as<float>();
    if(ac.containsKey("lon")) a.lon=ac["lon"].as<float>();
    int n=0;
	    if(_ri(ac,"gs",n)||_ri(ac,"speed",n)||_ri(ac,"spd",n)||_ri(ac,"tas",n)||_ri(ac,"ias",n)) a.spd=n;
	    if(_ri(ac,"track",n)||_ri(ac,"true_heading",n)||_ri(ac,"mag_heading",n))                 a.hdg=n;
	    if(_ri(ac,"alt_baro",n)||_ri(ac,"alt_geom",n)||_ri(ac,"altitude",n))                     a.alt=n;
    else if(ac["alt_baro"].is<const char*>()&&
            String(ac["alt_baro"].as<const char*>())=="ground") a.alt=-1;
    a.lastSeen=millis();
  }

  static void _fill(Aircraft& a, JsonObject ac) {
    String fl=ac["flight"]|""; fl.trim(); if(fl.length()) a.flight=fl;
    if(ac["r"].is<const char*>())    a.reg      =ac["r"].as<String>();
    if(ac["t"].is<const char*>())    a.type     =ac["t"].as<String>();
    if(ac["desc"].is<const char*>()) a.operator_=ac["desc"].as<String>();
    if(ac.containsKey("lat")) a.lat=ac["lat"].as<float>();
    if(ac.containsKey("lon")) a.lon=ac["lon"].as<float>();
    if(ac["squawk"].is<const char*>()) a.sq=ac["squawk"].as<String>();
    int n=0;
	    if(_ri(ac,"gs",n)||_ri(ac,"speed",n)||_ri(ac,"spd",n)||_ri(ac,"tas",n)||_ri(ac,"ias",n)) a.spd=n;
	    if(_ri(ac,"track",n)||_ri(ac,"true_heading",n)||_ri(ac,"mag_heading",n))                 a.hdg=n;
	    if(_ri(ac,"baro_rate",n)||_ri(ac,"geom_rate",n)||_ri(ac,"vert_rate",n))                  a.vs=n;
	    if(_ri(ac,"alt_baro",n)||_ri(ac,"alt_geom",n)||_ri(ac,"altitude",n))                     a.alt=n;
	    else if(ac["alt_baro"].is<const char*>()&&
	            String(ac["alt_baro"].as<const char*>())=="ground") a.alt=-1;
	    if(ac["squawk"].is<const char*>()) a.sq=ac["squawk"].as<String>();
	    else if(_ri(ac,"squawk",n)) a.sq=String(n);
	    a.lastSeen=millis();
	  }

  static bool _ri(JsonObject ac, const char* k, int& v) {
    if(!ac.containsKey(k)) return false;
    JsonVariant j=ac[k];
    if(j.is<int>())        { v=j.as<int>(); return true; }
    if(j.is<float>())      { v=(int)j.as<float>(); return true; }
    if(j.is<const char*>()){String s=j.as<String>();if(!s.length()||s=="ground")return false;v=s.toInt();return true;}
    return false;
  }

  static void _geo(Aircraft& a) {
    float R=6371.0f,dlat=radians(a.lat-obsLat),dlon=radians(a.lon-obsLon);
    float x=sinf(dlat/2)*sinf(dlat/2)+cosf(radians(obsLat))*cosf(radians(a.lat))*sinf(dlon/2)*sinf(dlon/2);
    a.dist=R*2*atan2f(sqrtf(x),sqrtf(1-x));
    float dy=sinf(dlon)*cosf(radians(a.lat));
    float dx=cosf(radians(obsLat))*sinf(radians(a.lat))-sinf(radians(obsLat))*cosf(radians(a.lat))*cosf(dlon);
    a.brg=fmodf(degrees(atan2f(dy,dx))+360.0f,360.0f);
  }

  static void _sort(Aircraft* a, int n) {
    for(int i=0;i<n-1;i++) for(int j=i+1;j<n;j++)
      if(a[j].dist<a[i].dist){Aircraft t=a[i];a[i]=a[j];a[j]=t;}
  }
};
