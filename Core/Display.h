#pragma once

#include <M5Unified.h>
#include "Theme.h"

class Display {
public:
  static M5Canvas cv;
  static int W, H;

  static void begin() {
    M5.Display.setRotation(1);
    W = M5.Display.width();
    H = M5.Display.height();
    cv.createSprite(W, H);
    cv.setTextWrap(false);
  }

  static void push()  { cv.pushSprite(0, 0); }
  static void clear() { cv.fillScreen(C_BG); }

  static int bodyTop()  { return SB_H; }
  static int bodyBot()  { return H - HB_H; }
  static int bodyH()    { return bodyBot() - bodyTop(); }

  static void setBright(uint8_t v) { M5.Display.setBrightness(v); }
};
