#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  // The Arduino IDE compiles all files... but this code should only be
  // compiled if the target is a Circuit Playground board

#include "intf_circuitplayground.h"

#include <Adafruit_CircuitPlayground.h>


namespace {

  class CircuitPlaygroundInterface : public InterfaceBase {
    void setup() {
      CircuitPlayground.begin();
    }

    Event loop() {
      return CircuitPlayground.leftButton() ? Event::startFlash : Event::idle;
   }

    void startMsg(const char* msg) {
      CircuitPlayground.clearPixels();
      CircuitPlayground.setPixelColor(9, 100, 100, 30);
    }
    void statusMsg(const char* msg) { }
    void errorMsg(const char* msg) {
      CircuitPlayground.clearPixels();
      CircuitPlayground.setPixelColor(9, 100, 30, 30);
    }
    void clearMsg() {
      CircuitPlayground.clearPixels();
    }
    void binaries(
      size_t bootSize, const char* bootName,
      size_t appSize, const char* appName)
      {
        CircuitPlayground.clearPixels();
        if (bootSize > 0)   CircuitPlayground.setPixelColor(0, 30, 100, 30);
        if (appSize > 0)    CircuitPlayground.setPixelColor(1, 30, 100, 30);
      }

    void progress(Burn phase, size_t done, size_t size) {
      int n = (done * 10 + done / 2) / size;

      CircuitPlayground.strip.clear();
      switch (phase) {
        case Burn::programming:
          for (int i = 0; (i + 1) <= n; ++i)
            CircuitPlayground.strip.setPixelColor(i, 100, 30, 100);
            break;

        case Burn::verifying:
          for (int i = 0; (i + 1) <= n; ++i)
            CircuitPlayground.strip.setPixelColor(i, 100, 50, 30);
          break;

        case Burn::complete:
          CircuitPlayground.strip.setPixelColor(4, 100, 100, 100);
          CircuitPlayground.strip.setPixelColor(5, 100, 100, 100);
          break;
      }
      CircuitPlayground.strip.show();

      if (phase == Burn::complete) {
        CircuitPlayground.speaker.enable(true);
        CircuitPlayground.playTone(180, 200, true);
        CircuitPlayground.playTone(240, 100, true);
        CircuitPlayground.playTone(360, 100, true);
        CircuitPlayground.playTone(240, 200, true);
        CircuitPlayground.speaker.enable(false);
      }
    }

  };

  CircuitPlaygroundInterface circuitPlaygroundInterface_;
}

Interface& circuitPlaygroundInterface = circuitPlaygroundInterface_;

#endif // ADAFRUIT_CIRCUITPLAYGROUND_M0
