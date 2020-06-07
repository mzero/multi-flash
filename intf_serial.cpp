#include "intf_serial.h"

#include <Arduino.h>

#include "config.h"

namespace {

  void ready() {
    // Do this on demand, not in setup, otherwise, the USB system will come
    // up before other descriptors have in been added.
    #ifdef MF_WAIT_FOR_SERIAL
        while (!Serial) {
          delay(100);
        }
    #endif
  }

  class SerialInterface : public InterfaceBase {
  public:
    void setup() {
      Serial.begin(115200);
    }

    void startMsg(const char* msg) {
      ready();
      Serial.println();
      Serial.println();
      Serial.println(msg);
      for (auto i = strlen(msg); i; --i)
        Serial.print('-');
      Serial.println();
    }

    void statusMsg(const char* msg) {
      ready();
      Serial.println(msg);
    }

    void errorMsg(const char* msg) {
      ready();
      Serial.print("** ");
      Serial.println(msg);
    }

    void binaries(
      size_t bootSize, const char* bootName,
      size_t appSize, const char* appName)
    {
      ready();

      if (bootSize > 0) Serial.printf("> boot: %3dk %s\n", sizeInK(bootSize), bootName);
      else              Serial.printf("> boot: ---  no binary\n");

      if (appSize > 0)  Serial.printf("> app:  %3dk %s\n", sizeInK(appSize), appName);
      else              Serial.printf("> app:  ---  no binary\n");

      Serial.println();
    }

  private:
    Burn progressPhase = Burn::complete;
    int  progressPct = 0;

  public:
    void progress(Burn phase, size_t done, size_t size) {
      ready();

      if (phase != progressPhase) {
        progressPhase = phase;
        progressPct = 0;

        switch (phase) {
          case Burn::programming:   Serial.print("programming: ");  break;
          case Burn::verifying:     Serial.print("\nverifying:  "); break;
          case Burn::complete:      Serial.print("\ndone\n");       return;
        }
      }

      int pct = static_cast<int>((done * 10) / size) * 10;

      if (pct != progressPct) {
        progressPct = pct;

        Serial.printf("..%2d%%", pct);
      }
    }
  };

  SerialInterface serialInterface_;
}

Interface& serialInterface = serialInterface_;

