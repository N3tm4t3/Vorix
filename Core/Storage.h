#pragma once
#include <Arduino.h>
#include <Preferences.h>

class Storage {
public:
  static void begin()  { _p.begin("vorix", false); }

  static String getSSID()   { return _p.getString("ssid", ""); }
  static String getPass()   { return _p.getString("pass", ""); }
  static void   setSSID(const String& v) { _p.putString("ssid", v); }
  static void   setPass(const String& v) { _p.putString("pass", v); }
  static void   clearWiFi() { _p.remove("ssid"); _p.remove("pass"); }

  static float  getLat()    { return _p.getString("lat","0").toFloat(); }
  static float  getLon()    { return _p.getString("lon","0").toFloat(); }
  static int    getRadius() { int r=_p.getInt("radius",100); return(r<10||r>250)?100:r; }
  static void   setLat(float v)   { _p.putString("lat", String(v,6)); }
  static void   setLon(float v)   { _p.putString("lon", String(v,6)); }
  static void   setRadius(int v)  { _p.putInt("radius", v); }

  static int    getBright() { return _p.getInt("bright",180); }
  static void   setBright(int v)  { _p.putInt("bright", v); }

private:
  static Preferences _p;
};
