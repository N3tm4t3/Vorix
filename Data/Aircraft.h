#pragma once
#include <Arduino.h>

constexpr int MAX_AC    = 15;
constexpr int TRACK_MAX = 20;

struct TrackPt { float lat, lon; int alt; uint32_t ts; };

struct Aircraft {

  String hex, reg, type, operator_;

  String flight, flnum, airline, from, to, sq;

  int    alt=0, spd=0, vs=0, hdg=0;
  float  lat=0, lon=0;

  float  dist=0, brg=0;

  bool   routeLoaded=false;
  uint32_t lastSeen=0;

	  TrackPt  track[TRACK_MAX];
	  int      trackLen=0;
	  uint32_t lastTrackTs=0;

  // UI uses these targets to animate aircraft smoothly between network fetches.
	  float  prevLat=0, prevLon=0;
	  float  targLat=0, targLon=0;
  float  prevBrg=0, targBrg=0;
  float  prevHdg=0, targHdg=0;
  float  interpT=1.0f;
  uint8_t fadeAlpha=255;
	  bool   isNew=true;
	  bool   fadingOut=false;

  bool  emg()  const { return sq=="7700"||sq=="7600"||sq=="7500"; }
	  bool  gnd()  const { return alt==-1; }
	  String cs()  const { return flight.length()?flight:hex; }

  String emgLabel() const {
    if(sq=="7700") return "MAYDAY";
    if(sq=="7600") return "RADIO";
    if(sq=="7500") return "HIJACK";
    return "";
  }

  String phase() const {
    if(alt==-1){ if(spd<2)return "PARK"; if(spd<40)return "TAXI"; return "T/O"; }
    if(vs>500)  return "CLB";
    if(vs<-500) return "DSC";
    if(alt>20000&&abs(vs)<200) return "CRZ";
    if(alt<3000&&vs<-100) return "APPR";
    return "LVL";
  }

	  void startInterp() {
	    if(prevLat == 0.0f && prevLon == 0.0f) {
      prevLat = lat; prevLon = lon;
      prevBrg = brg; prevHdg = (float)hdg;
    } else {
      prevLat = (interpT < 1.0f) ? lerpF(prevLat, targLat, interpT) : targLat;
      prevLon = (interpT < 1.0f) ? lerpF(prevLon, targLon, interpT) : targLon;
      prevBrg = (interpT < 1.0f) ? lerpAngle(prevBrg, targBrg, interpT) : targBrg;
      prevHdg = (interpT < 1.0f) ? lerpAngle(prevHdg, targHdg, interpT) : targHdg;
    }
    targLat = lat; targLon = lon;
    targBrg = brg; targHdg = (float)hdg;
    interpT = 0.0f;
  }

  void stepInterp(float dt, float period) {
    if(period > 0.01f) interpT += dt / period;
    if(interpT > 1.0f) interpT = 1.0f;
	    if(fadingOut) {
	      fadeAlpha = (fadeAlpha > 18) ? (uint8_t)(fadeAlpha - 18) : 0;
	      return;
	    }

    if(isNew) {
      if(fadeAlpha < 255) fadeAlpha = (uint8_t)min(255, (int)fadeAlpha + 20);
      else isNew = false;
    }
  }

	  float ilat() const { return lerpF(prevLat, targLat, interpT); }
	  float ilon() const { return lerpF(prevLon, targLon, interpT); }
	  float ibrg() const { return lerpAngle(prevBrg, targBrg, interpT); }
  float ihdg() const { return lerpAngle(prevHdg, targHdg, interpT); }

  void addTrack() {
    if(lat==0&&lon==0) return;
    uint32_t now=millis();
    if(now-lastTrackTs<20000) return;
    lastTrackTs=now;
    if(trackLen<TRACK_MAX) { track[trackLen++]={lat,lon,alt,now}; return; }
    for(int i=0;i<TRACK_MAX-1;i++) track[i]=track[i+1];
    track[TRACK_MAX-1]={lat,lon,alt,now};
  }

private:
  static float lerpF(float a, float b, float t) { return a + (b-a)*t; }

  // Interpolates headings across north without spinning the long way around.
  static float lerpAngle(float a, float b, float t) {
    float d = b - a;
    while(d >  180.0f) d -= 360.0f;
    while(d < -180.0f) d += 360.0f;
    return fmodf(a + d*t + 360.0f, 360.0f);
  }
};
