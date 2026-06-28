#pragma once
#include "Display.h"
#include "Theme.h"
#include <M5Unified.h>

class Boot {
public:
  static void logo() {
    auto& cv=Display::cv;
    int W=Display::W,H=Display::H;
    cv.fillScreen(C_BG);

	    cv.setFont(FSB9); cv.setTextSize(2); cv.setTextDatum(MC_DATUM);
	    cv.setTextColor(C_ACCENT); cv.drawString("VORIX",W/2,H/2-9);
	    cv.setFont(F0); cv.setTextSize(1); cv.setTextColor(C_DIM);
	    cv.drawString("AVIATION COMPANION",W/2,H/2+14);
	    cv.setTextColor(C_DIM);
	    cv.drawString("by n3tm4t3",W/2,H/2+27);
	    Display::push();
	  }

  static void setup_() {
    auto& cv=Display::cv; int W=Display::W,H=Display::H;
    cv.fillScreen(C_BG);
    cv.fillRect(0,0,W,SB_H,C_ACCENT);
    cv.setFont(F0); cv.setTextColor(C_BG); cv.setTextDatum(MC_DATUM);
    cv.drawString("VORIX  SETUP",W/2,SB_H/2);
    int y=SB_H+8;
    auto ln=[&](const String& s,uint32_t c=C_TEXT2){
      cv.setTextColor(c); cv.setTextDatum(TL_DATUM); cv.drawString(s,6,y); y+=13;};
    ln("1. JOIN WIFI:",C_DIM); ln("   VORIX-SETUP",C_CYAN);
    y+=3; ln("2. OPEN BROWSER:",C_DIM); ln("   192.168.4.1",C_CYAN);
    y+=3; ln("3. FILL DETAILS + SAVE",C_GOLD);
    Display::push();
  }

  static void connecting(const String& ssid) {
    auto& cv=Display::cv;
    int W=Display::W,H=Display::H;
    cv.fillScreen(C_BG);

    cv.setFont(FSB9); cv.setTextSize(2); cv.setTextDatum(MC_DATUM);
    cv.setTextColor(C_ACCENT); cv.drawString("VORIX",W/2,29);
    cv.setFont(F0); cv.setTextSize(1);
    cv.setTextColor(C_DIM); cv.drawString("CONNECTING TO",W/2,55);
    String shown=ssid.length()>28 ? ssid.substring(0,25)+"..." : ssid;
    cv.setTextColor(C_TEXT); cv.drawString(shown,W/2,69);
    cv.drawRoundRect(50,89,W-100,5,2,C_ACCENT_DIM);
    Display::push();
  }

  static void progress(int pct) {
    auto& cv=Display::cv;
    int W=Display::W,H=Display::H;

    int x=52,w=W-104,y=91;
    cv.fillRect(x,y,w,1,C_BG);
    int filled=(w*constrain(pct,0,100))/100;
    if(filled>0) cv.drawFastHLine(x,y,filled,C_ACCENT);
    Display::push();
  }

  static void connected(const String& ip) {
    auto& cv=Display::cv;
    int W=Display::W,H=Display::H;
    cv.fillScreen(C_BG);

    cv.setTextDatum(MC_DATUM);
    cv.setFont(FSB9); cv.setTextSize(2); cv.setTextColor(C_ACCENT);
    cv.drawString("CONNECTED",W/2,H/2-14);
    for(int frame=1;frame<=4;frame++) {
      int width=frame*16;
      cv.fillRect(W/2-width/2,H/2+7,width,2,C_ACCENT);
      Display::push();
      delay(70);
    }
    cv.setFont(FSB9); cv.setTextSize(1); cv.setTextColor(C_TEXT);
    cv.drawString(ip,W/2,H/2+28);
    Display::push();
    delay(850);
  }

  static void error(const String& msg) {
    auto& cv=Display::cv; int W=Display::W,H=Display::H;
    cv.fillScreen(C_BG);
    cv.fillRect(0,0,W,SB_H,C_NEG);
    cv.setFont(F0); cv.setTextColor(C_BG); cv.setTextDatum(MC_DATUM);
    cv.drawString("ERROR",W/2,SB_H/2);
    cv.setFont(FS9); cv.setTextColor(C_NEG); cv.drawString(msg,W/2,H/2-8);
    cv.setFont(F0); cv.setTextColor(C_DIM);
    cv.drawString("Hold A (2s) to reconfigure",W/2,H/2+8);
    Display::push();
  }

};
