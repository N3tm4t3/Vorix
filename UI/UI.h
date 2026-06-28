#pragma once

#include <M5Unified.h>
#include "../Core/Display.h"
#include "../Core/Theme.h"
#include "../Data/AircraftFetcher.h"
#include "../Data/FetchTask.h"
#include "../Core/Storage.h"
#include "../Core/Network.h"

enum class Screen { RADAR, LIST, DETAIL, SETTINGS };

enum class SetItem {
  WIFI_SSID=0, WIFI_PASS, WIFI_IP, WIFI_FORGET,
  LOC_LAT, LOC_LON, LOC_RADIUS,
  SYS_BRIGHT, SYS_ABOUT,
  COUNT
};
static const char* SET_LABELS[] = {
  "SSID","PASSWORD","IP ADDRESS","FORGET WIFI",
  "LATITUDE","LONGITUDE","RADIUS",
  "BRIGHTNESS","ABOUT"
};

class UI {
public:
  Screen   screen     = Screen::RADAR;
  int      selIdx     = 0;
  int      detPage    = 0;
  bool     dirty      = true;
  float    sweep      = 0;
  uint32_t lastTickMs = 0;

  int      setRow     = 0;
  bool     setEditing = false;
  Screen   _prevScreen = Screen::RADAR;

  void onALong() { _goSettings(); }

  void onBLong() {
    _beep(600,80);
    FetchTask::clearFocus();
    screen=Screen::RADAR;
    dirty=true;
  }

  void onCLong() {
    if(screen==Screen::SETTINGS) {
      _beep(600,80);
      screen=_prevScreen;
      if(screen==Screen::DETAIL) FetchTask::setFocus(selIdx);
      else FetchTask::clearFocus();
      dirty=true;
    }
  }

  void onA() {
    _beep(1800,25);
    switch(screen) {
      case Screen::RADAR:
      case Screen::LIST: {

	        int cnt = _count();
        if(cnt == 0) {

          dirty=true;
          return;
        }
        _clamp();
        screen=Screen::DETAIL;
        detPage=0;
        FetchTask::setFocus(selIdx);
        break;
      }
      case Screen::DETAIL:
        detPage=(detPage+1)%4;
        break;
      case Screen::SETTINGS:
        _settingsSelect();
        break;
    }
    dirty=true;
  }

  void onB() {
    _beep(1500,20);
    switch(screen) {
      case Screen::DETAIL:
        detPage=(detPage+1)%4;
        break;
	      case Screen::SETTINGS:
	        if(!setEditing) setRow=max(setRow-1,0);
	        else _settingsAdjust(-1);
	        break;
      default: {
	        int cnt=_count();
        if(cnt>0) selIdx=(selIdx+1)%cnt;
        break;
      }
    }
    dirty=true;
  }

  void onC() {
    _beep(1500,20);
    switch(screen) {
      case Screen::DETAIL:
        detPage=(detPage+3)%4;
        break;
	      case Screen::SETTINGS:
	        if(!setEditing) setRow=min(setRow+1,(int)SetItem::COUNT-1);
	        else _settingsAdjust(+1);
	        break;
      default: {
	        int cnt=_count();
        if(cnt>0) selIdx=(selIdx==0)?cnt-1:selIdx-1;
        break;
      }
    }
    dirty=true;
  }

  void markDirty() { dirty=true; }

  bool tick(uint32_t now) {
    float dt = lastTickMs ? (now-lastTickMs)/1000.0f : 0.067f;
    lastTickMs = now;
    dt = min(dt,0.15f);
    sweep = fmodf(sweep + 55.0f*dt, 360.0f);

    {
      const float period = 15.0f;
      FetchTask::lock();
      for(int i=0; i<AircraftFetcher::count; i++)
        AircraftFetcher::list[i].stepInterp(dt, period);
      FetchTask::unlock();
    }

    if(screen==Screen::RADAR || screen==Screen::DETAIL) dirty=true;

    if(!dirty) return false;
    dirty=false;
    _draw();
    Display::push();
    return true;
  }

private:
	  M5Canvas& cv() const { return Display::cv; }
	  void _beep(int f,int d) { M5.Speaker.tone(f,d); }

	  int _count() {
	    FetchTask::lock();
	    int cnt = AircraftFetcher::count;
	    FetchTask::unlock();
	    return cnt;
	  }

	  void _clamp() {
	    int cnt=_count();
    if(cnt==0)         selIdx=0;
    else if(selIdx>=cnt) selIdx=cnt-1;
  }

  void _goSettings() {
    _beep(1200,40);
    _prevScreen=screen;
    if(screen==Screen::DETAIL) FetchTask::clearFocus();
    screen=Screen::SETTINGS;
    setRow=0; setEditing=false;
    dirty=true;
  }

  void _settingsSelect() {
    SetItem item=(SetItem)setRow;
    switch(item) {
      case SetItem::WIFI_FORGET:
        Storage::clearWiFi(); _beep(400,200); delay(500); ESP.restart(); break;
      case SetItem::SYS_BRIGHT:
      case SetItem::LOC_LAT:
      case SetItem::LOC_LON:
      case SetItem::LOC_RADIUS:
        setEditing=!setEditing; break;
      default: break;
    }
  }

  void _settingsAdjust(int dir) {
    switch((SetItem)setRow) {
      case SetItem::SYS_BRIGHT: {
        int b=max(20,min(255,Storage::getBright()+dir*10));
        Storage::setBright(b); Display::setBright((uint8_t)b); break;
      }
      case SetItem::LOC_LAT: {
        float v=Storage::getLat()+dir*0.1f; Storage::setLat(v);
        FetchTask::setLocation(Storage::getLat(),Storage::getLon(),Storage::getRadius());
        FetchTask::triggerNow(); break;
      }
      case SetItem::LOC_LON: {
        float v=Storage::getLon()+dir*0.1f; Storage::setLon(v);
        FetchTask::setLocation(Storage::getLat(),Storage::getLon(),Storage::getRadius());
        FetchTask::triggerNow(); break;
      }
      case SetItem::LOC_RADIUS: {
        int v=max(10,min(250,Storage::getRadius()+dir*10));
        Storage::setRadius(v);
        FetchTask::setLocation(Storage::getLat(),Storage::getLon(),v);
        FetchTask::triggerNow(); break;
      }
      default: break;
    }
  }

  void _draw() {
    _clamp();
    Display::clear();
    switch(screen) {
      case Screen::RADAR:    _drawRadar();    break;
      case Screen::LIST:     _drawList();     break;
      case Screen::DETAIL:   _drawDetail();   break;
      case Screen::SETTINGS: _drawSettings(); break;
    }
  }

  void _drawRadar() {
    int W=Display::W, H=Display::H;
    int r  = min(55, H/2-7);
    int cx = 58, cy = H/2;

    for(int i=1;i<=3;i++)
      cv().drawCircle(cx,cy,r*i/3,C_LINE);
    cv().drawFastHLine(cx-r,cy,r*2,C_LINE);
    cv().drawFastVLine(cx,cy-r,r*2,C_LINE);

    for(int t=3;t>=1;t--){
      float ta=radians(sweep-t*12.0f);
      uint32_t col = t==1 ? 0x3018 : t==2 ? 0x180C : 0x0804;
      cv().drawLine(cx,cy, cx+(int)(r*sinf(ta)), cy-(int)(r*cosf(ta)), col);
    }

    {
      float sa=radians(sweep);
      cv().drawLine(cx,cy, cx+(int)(r*sinf(sa)), cy-(int)(r*cosf(sa)), C_ACCENT);
    }

    cv().fillCircle(cx,cy,4,C_ACCENT);
    cv().fillCircle(cx,cy,2,C_TEXT);

    FetchTask::lock();
    int cnt=AircraftFetcher::count;
    int airborne=0, ground=0;
    float nearestDist=99999.0f;
    String nearestName="---";

    if(cnt==0) {

      cv().setFont(F0); cv().setTextSize(1);
      cv().setTextColor(C_DIM); cv().setTextDatum(MC_DATUM);
      cv().drawString("no traffic", cx, cy+r/2+6);
    }

	    for(int i=0;i<cnt;i++){
	      const Aircraft& ac=AircraftFetcher::list[i];
	      if(ac.gnd()) ground++; else airborne++;
	      if(ac.lat==0 && ac.lon==0) continue;
	      if(ac.dist>=0 && ac.dist<nearestDist) {
	        nearestDist=ac.dist;
	        nearestName=ac.cs();
	      }

	      float dist, brg;
	      _smoothGeo(ac, AircraftFetcher::obsLat, AircraftFetcher::obsLon, dist, brg);
	      float norm = min(dist / (float)max(AircraftFetcher::obsRadius,1), 1.0f);
	      float brgR = radians(brg);
	      int   plotR = r-9;
	      int   px   = cx + (int)(plotR * norm * sinf(brgR));
	      int   py   = cy - (int)(plotR * norm * cosf(brgR));

      bool sel = (i==selIdx);
      uint32_t col = ac.emg() ? C_NEG : (sel ? C_CYAN : C_POS);

      if(ac.fadeAlpha < 200) col = _dimColor(col, ac.fadeAlpha);

      _acIcon(px, py, (int)ac.ihdg(), col, sel);
    }
    FetchTask::unlock();

    _drawRadarStats(cnt,airborne,ground,nearestName,nearestDist);
  }

  void _drawRadarStats(int cnt,int airborne,int ground,const String& nearest,
                       float nearestDist) {
    int W=Display::W,H=Display::H;
    int left=120,cx=(left+W)/2;
    cv().drawFastVLine(left-3,8,H-16,C_LINE);

    cv().setTextDatum(MC_DATUM); cv().setTextSize(1);
    cv().setFont(F0); cv().setTextColor(C_DIM);
    cv().drawString("AIRCRAFT NEARBY",cx,13);
    cv().setFont(FSB9); cv().setTextSize(2); cv().setTextColor(C_ACCENT);
    cv().drawString(String(cnt),cx,35);

    cv().setFont(F0); cv().setTextSize(1); cv().setTextColor(C_TEXT2);
    cv().drawString("AIR "+String(airborne)+"   GND "+String(ground),cx,57);
    cv().setTextColor(C_DIM);
    cv().drawString("RANGE  "+String(AircraftFetcher::obsRadius)+" KM",cx,70);
    cv().drawFastHLine(left+7,80,W-left-14,C_LINE);

    cv().drawString("NEAREST",cx,91);
    String name=nearest.length()>14 ? nearest.substring(0,14) : nearest;
    cv().setTextColor(C_TEXT); cv().drawString(cnt?name:"---",cx,104);
    cv().setTextColor(C_CYAN);
    String distance=cnt&&nearestDist<99998.0f ? String(nearestDist,1)+" KM":"--.- KM";
    cv().drawString(distance,cx,117);
  }

  void _acIcon(int cx, int cy, int hdgDeg, uint32_t col, bool sel) {
    int rad = sel ? 3 : 2;
    cv().fillCircle(cx, cy, rad, col);
    if(sel) cv().drawCircle(cx, cy, rad+2, col);

    float a  = radians(hdgDeg);
    int   tl = sel ? 10 : 7;
    cv().drawLine(cx, cy,
                  cx+(int)(tl*sinf(a)),
                  cy-(int)(tl*cosf(a)), col);
  }

	  static uint32_t _dimColor(uint32_t col565, uint8_t alpha) {
	    uint32_t r = ((col565 >> 11) & 0x1F) * alpha / 255;
	    uint32_t g = ((col565 >>  5) & 0x3F) * alpha / 255;
	    uint32_t b = ( col565        & 0x1F) * alpha / 255;
	    return (r<<11)|(g<<5)|b;
	  }

	  static void _smoothGeo(const Aircraft& ac, float obsLat, float obsLon,
	                         float& dist, float& brg) {
	    float lat = ac.ilat();
	    float lon = ac.ilon();

	    float R=6371.0f,dlat=radians(lat-obsLat),dlon=radians(lon-obsLon);
	    float x=sinf(dlat/2)*sinf(dlat/2)+cosf(radians(obsLat))*cosf(radians(lat))*sinf(dlon/2)*sinf(dlon/2);
	    dist=R*2*atan2f(sqrtf(x),sqrtf(1-x));
	    float dy=sinf(dlon)*cosf(radians(lat));
	    float dx=cosf(radians(obsLat))*sinf(radians(lat))-sinf(radians(obsLat))*cosf(radians(lat))*cosf(dlon);
	    brg=fmodf(degrees(atan2f(dy,dx))+360.0f,360.0f);
	  }

  void _drawList() {
    int W=Display::W, H=Display::H;
    int top=2;
    const int RH=17;
    int vis=(H-top)/RH;

    FetchTask::lock();
    int cnt=AircraftFetcher::count;

    if(cnt==0){
      FetchTask::unlock();
      cv().setFont(FS9); cv().setTextColor(C_DIM); cv().setTextDatum(MC_DATUM);
      cv().drawString(FetchTask::fetching?"Fetching...":"No traffic", W/2, H/2-8);
      cv().setFont(F0);
      cv().drawString(String(AircraftFetcher::obsRadius)+"km radius", W/2, H/2+8);
      return;
    }

    int scroll=max(0,min(selIdx-vis/2, cnt-vis));
    for(int i=scroll; i<cnt&&i<scroll+vis; i++){
      const Aircraft& ac=AircraftFetcher::list[i];
      int y=top+(i-scroll)*RH;
      bool sel=(i==selIdx);
      if(sel){ cv().fillRect(0,y,W,RH,C_ACCENT_DIM); cv().fillRect(0,y,2,RH,C_ACCENT); }
      else if(ac.emg()) cv().fillRect(0,y,W,RH,0x2000);

      cv().setFont(F0); cv().setTextSize(1);
      cv().setTextColor(ac.emg()?C_NEG:(sel?C_CYAN:C_TEXT));
      cv().setTextDatum(ML_DATUM);
      String cs=ac.cs(); if(cs.length()>7)cs=cs.substring(0,7);
      cv().drawString(cs,5,y+RH/2);
      cv().setTextColor(C_DIM);
      cv().drawString(ac.type.length()?ac.type:"---",58,y+RH/2);
      String altS=ac.gnd()?"GND":String(ac.alt/100)+"FL";
      cv().setTextColor(ac.vs>200?C_POS:ac.vs<-200?C_NEG:C_TEXT2);
      cv().setTextDatum(MR_DATUM);
      cv().drawString(altS,W-3,y+RH/2);
      cv().drawFastHLine(0,y+RH-1,W,C_LINE);
    }
    FetchTask::unlock();

    if(cnt>vis){
      int th=max(4,H*vis/cnt);
      int ty=H*scroll/cnt;
      cv().fillRect(W-2,ty,2,th,C_ACCENT_DIM);
    }
  }

  void _drawDetail() {
    FetchTask::lock();
    int cnt=AircraftFetcher::count;

    if(cnt==0 || selIdx>=cnt){
      FetchTask::unlock();
      screen=Screen::RADAR;
      FetchTask::clearFocus();
      dirty=true;
      _drawRadar();
      return;
    }

    Aircraft ac=AircraftFetcher::list[selIdx];
    FetchTask::unlock();

    int W=Display::W, top=2;

    for(int i=0;i<4;i++)
      cv().fillCircle(W/2-9+i*6, Display::H-6, i==detPage?3:2,
                      i==detPage?C_ACCENT:C_DIM);

	    switch(detPage){
	      case 0: _detOverview(ac,W,top); break;
	      case 1: _detNav(ac,W,top);      break;
	      case 2: _detIdent(ac,W,top);    break;
	      case 3: _detRoute(ac,W,top);    break;
	    }
  }

  void _row(int y, int W, const char* lbl, const String& val, uint32_t vc=C_TEXT) {
    cv().setFont(F0); cv().setTextSize(1);
    cv().setTextColor(C_DIM);  cv().setTextDatum(ML_DATUM); cv().drawString(lbl,3,y);
    cv().setTextColor(vc);     cv().setTextDatum(MR_DATUM); cv().drawString(val,W-3,y);
  }

  void _detOverview(const Aircraft& ac, int W, int top) {
    cv().setFont(FSB9); cv().setTextSize(1);
    cv().setTextColor(ac.emg()?C_NEG:C_CYAN); cv().setTextDatum(TL_DATUM);
    cv().drawString(ac.cs(),3,top);
    cv().setTextColor(C_GOLD); cv().setTextDatum(TR_DATUM);
    cv().drawString(ac.phase(),W-3,top);
    int y=top+16;
    if(ac.emg()){
      cv().fillRect(0,y,W,9,C_NEG);
      cv().setFont(F0); cv().setTextColor(C_TEXT); cv().setTextDatum(MC_DATUM);
      cv().drawString(ac.emgLabel()+" SQK "+ac.sq,W/2,y+5); y+=11;
    }
    cv().drawFastHLine(0,y,W,C_LINE); y+=4;
    const int MH=14;
    String altS=ac.gnd()?"GND":String(ac.alt)+"ft";
    String vsS=(ac.vs>0?"+":"")+String(ac.vs)+"fpm";
    uint32_t vsC=ac.vs>200?C_POS:ac.vs<-200?C_NEG:C_TEXT;
    cv().setFont(F0); cv().setTextSize(1);
    cv().setTextColor(C_DIM); cv().setTextDatum(ML_DATUM);
    cv().drawString("ALT",3,y+4); cv().setTextDatum(MC_DATUM); cv().drawString("V/S",W/2+2,y+4);
    cv().setTextColor(C_TEXT); cv().setTextDatum(ML_DATUM); cv().drawString(altS,3,y+12);
    cv().setTextColor(vsC); cv().setTextDatum(MC_DATUM); cv().drawString(vsS,W/2+2,y+12);
    y+=MH+8;
    cv().setTextColor(C_DIM); cv().setTextDatum(ML_DATUM);
    cv().drawString("GS",3,y+4); cv().setTextDatum(MC_DATUM); cv().drawString("HDG",W/2+2,y+4);
    cv().setTextColor(C_TEXT); cv().setTextDatum(ML_DATUM); cv().drawString(String(ac.spd)+"kt",3,y+12);
    cv().setTextDatum(MC_DATUM); cv().drawString(String(ac.hdg),W/2+2,y+12); y+=MH+8;
    cv().drawFastHLine(0,y,W,C_LINE); y+=4;
    cv().setTextColor(C_CYAN); cv().setTextDatum(ML_DATUM);
    cv().drawString("DIST "+String(ac.dist,1)+"km",3,y+4);
    cv().setTextColor(C_DIM); cv().setTextDatum(MR_DATUM);
    cv().drawString(ac.reg.length()?ac.reg+"/"+ac.type:ac.type,W-3,y+4);
  }

	  void _detNav(const Aircraft& ac, int W, int top) {
	    cv().setFont(F0); cv().setTextColor(C_ACCENT); cv().setTextDatum(TL_DATUM);
	    cv().drawString("NAVIGATION",3,top);
	    if(FetchTask::fetching && ((millis()/250)%2==0))
	      cv().fillCircle(W/2, top+4, 2, C_WARN);
	    int y=top+13; const int RH=12;
	    _row(y,W,"ALTITUDE",  ac.gnd()?"ON GND":String(ac.alt)+" ft"); y+=RH;
	    _row(y,W,"VERT SPD",  (ac.vs>0?"+":"")+String(ac.vs)+" fpm",
	         ac.vs>300?C_POS:ac.vs<-300?C_NEG:C_TEXT); y+=RH;
	    _row(y,W,"GRND SPD",  String(ac.spd)+" kt"); y+=RH;
	    _row(y,W,"HEADING",   String(ac.hdg)); y+=RH;
	    _row(y,W,"DISTANCE",  String(ac.dist,1)+" km", C_CYAN); y+=RH;
	    _row(y,W,"BEARING",   String((int)ac.brg)); y+=RH;
	    _row(y,W,"SQUAWK",    ac.sq.length()?ac.sq:"----",
	         ac.emg()?C_NEG:C_TEXT); y+=RH;
	    _row(y,W,"LAT",       String(ac.lat,4)); y+=RH;
	    _row(y,W,"LON",       String(ac.lon,4));
	  }

	  void _detIdent(const Aircraft& ac, int W, int top) {
	    cv().setFont(F0); cv().setTextColor(C_ACCENT); cv().setTextDatum(TL_DATUM);
	    cv().drawString("IDENTITY",3,top);
	    int y=top+14; const int RH=13;
	    String cs=ac.flight.length()?ac.flight:ac.hex;
	    if(ac.flnum.length()) cs+="/"+ac.flnum;
	    _row(y,W,"CALLSIGN",  cs, C_CYAN); y+=RH;
	    _row(y,W,"ICAO HEX",  ac.hex, C_DIM); y+=RH;
	    _row(y,W,"REG",       ac.reg.length()?ac.reg:"---"); y+=RH;
	    _row(y,W,"TYPE",      ac.type.length()?ac.type:"---"); y+=RH;
	    _row(y,W,"OPERATOR",  ac.operator_.length()?ac.operator_:"---"); y+=RH;

	    uint32_t lsec = (ac.lastSeen > 0) ? (millis()-ac.lastSeen)/1000 : 0;
	    uint32_t lsCol = lsec<3 ? C_POS : lsec<10 ? C_WARN : C_NEG;
	    _row(y,W,"UPDATED", String(lsec)+"s ago", lsCol); y+=RH;
	    _row(y,W,"DIST",      String(ac.dist,1)+" km", C_CYAN);
	  }

	  void _detRoute(const Aircraft& ac, int W, int top) {
	    cv().setFont(F0); cv().setTextColor(C_ACCENT); cv().setTextDatum(TL_DATUM);
	    cv().drawString("ROUTE",3,top);
	    int y=top+14; const int RH=13;
	    if(!ac.routeLoaded){
	      cv().setTextColor(C_DIM); cv().setTextDatum(MC_DATUM);
	      cv().drawString("Loading...",W/2,y+20); return;
	    }
	    if(ac.from.length()||ac.to.length()){
	      cv().setFont(FSB9); cv().setTextColor(C_GOLD); cv().setTextDatum(MC_DATUM);
	      cv().drawString((ac.from.length()?ac.from:"???")+">>"+(ac.to.length()?ac.to:"???"),W/2,y+7);
	      y+=19; cv().setFont(F0);
	      cv().drawFastHLine(3,y,W-6,C_LINE); y+=7;
	    }
	    _row(y,W,"AIRLINE", ac.airline.length()?ac.airline:"---"); y+=RH;
	    _row(y,W,"IATA",    ac.flnum.length()?ac.flnum:"---"); y+=RH;
	    _row(y,W,"ORIGIN",  ac.from.length()?ac.from:"---"); y+=RH;
	    _row(y,W,"DEST",    ac.to.length()?ac.to:"---");
	  }

  void _drawSettings() {
    int W=Display::W, H=Display::H;
    int top=2; const int RH=14;
    cv().setFont(F0); cv().setTextSize(1);
    int items=(int)SetItem::COUNT;
    int vis=(H-top)/RH;
    int scroll=max(0,min(setRow-vis/2,items-vis));

    for(int i=scroll; i<items&&i<scroll+vis; i++){
      int y=top+(i-scroll)*RH;
      bool sel=(i==setRow), editing=(sel&&setEditing);
      if(sel)     cv().fillRect(0,y,W,RH,C_ACCENT_DIM);
      if(editing) cv().fillRect(0,y,2,RH,C_WARN);
      else if(sel)cv().fillRect(0,y,2,RH,C_ACCENT);

      cv().setTextColor(sel?C_TEXT:C_TEXT2);
      cv().setTextDatum(ML_DATUM);
      cv().drawString(SET_LABELS[i],6,y+RH/2);

      String val="";
      switch((SetItem)i){
	        case SetItem::WIFI_SSID:   val=Storage::getSSID(); if(!val.length())val="(none)"; break;
	        case SetItem::WIFI_PASS:   val="......"; break;
	        case SetItem::WIFI_IP:     val=Network::ip(); if(!val.length())val="--"; break;
	        case SetItem::WIFI_FORGET: val="HOLD A"; break;
        case SetItem::LOC_LAT:     val=String(Storage::getLat(),4); break;
        case SetItem::LOC_LON:     val=String(Storage::getLon(),4); break;
        case SetItem::LOC_RADIUS:  val=String(Storage::getRadius())+"km"; break;
        case SetItem::SYS_BRIGHT:  val=String(Storage::getBright()); break;
        case SetItem::SYS_ABOUT:   val="v2.4 "+String(ESP.getFreeHeap()/1024)+"kB"; break;
        default: break;
      }
      cv().setTextColor(editing?C_WARN:(sel?C_CYAN:C_DIM));
      cv().setTextDatum(MR_DATUM);
      cv().drawString(val,W-4,y+RH/2);
      cv().drawFastHLine(0,y+RH-1,W,C_LINE);
    }

    if(items>vis){
      int th=max(3,H*vis/items);
      int ty=H*scroll/items;
      cv().fillRect(W-2,ty,2,th,C_ACCENT_DIM);
    }
    if(setEditing){
      cv().setTextColor(C_WARN); cv().setTextDatum(TL_DATUM);
      cv().drawString("< B:- C:+ A:save >",3,H-10);
    }
  }
};
