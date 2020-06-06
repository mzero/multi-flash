#include "intf_oledfeatherwing.h"

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



// OLED FeatherWing buttons map to different pins depending on board:
#if defined(ESP8266)
  #define OLED_FEATHERWING_BUTTON_A  0
  #define OLED_FEATHERWING_BUTTON_B 16
  #define OLED_FEATHERWING_BUTTON_C  2
#elif defined(ESP32)
  #define OLED_FEATHERWING_BUTTON_A 15
  #define OLED_FEATHERWING_BUTTON_B 32
  #define OLED_FEATHERWING_BUTTON_C 14
#elif defined(ARDUINO_STM32_FEATHER)
  #define OLED_FEATHERWING_BUTTON_A PA15
  #define OLED_FEATHERWING_BUTTON_B PC7
  #define OLED_FEATHERWING_BUTTON_C PC5
#elif defined(TEENSYDUINO)
  #define OLED_FEATHERWING_BUTTON_A  4
  #define OLED_FEATHERWING_BUTTON_B  3
  #define OLED_FEATHERWING_BUTTON_C  8
#elif defined(ARDUINO_FEATHER52832)
  #define OLED_FEATHERWING_BUTTON_A 31
  #define OLED_FEATHERWING_BUTTON_B 30
  #define OLED_FEATHERWING_BUTTON_C 27
#else // 32u4, M0, M4, nrf52840 and 328p
  #define OLED_FEATHERWING_BUTTON_A  9
  #define OLED_FEATHERWING_BUTTON_B  6
  #define OLED_FEATHERWING_BUTTON_C  5
#endif


namespace {

  class OledFeatherwing : public InterfaceBase {
  public:
    void setup() {
      // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
      display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
      display.cp437();
      display.clearDisplay();

      display.setTextSize(1);
      display.setFont();
      display.setTextColor(WHITE);
      display.setTextWrap(false);

      display.println("Multi-Flash");
      display.display();

      pinMode(OLED_FEATHERWING_BUTTON_A, INPUT_PULLUP);
      lastButtonState = HIGH;
      buttonValidAt = 0;
    }

    Event loop() {
      auto b = digitalRead(OLED_FEATHERWING_BUTTON_A);
      if (b == lastButtonState) {
        if (buttonValidAt > 0) {
          if (buttonValidAt <= millis()) {
            buttonValidAt = 0;
            if (b == LOW) {
              return Event::startFlash;
            }
          } // else still waiting for valid time
        } // else long since reported this
      } else {
        lastButtonState = b;
        buttonValidAt = millis() + 50; // debounce time
      }

      return Event::idle;
    }

    void startMsg(const char* msg)  { textLine(1, false, msg); }
    void statusMsg(const char* msg) { textLine(4, false, msg); }
    void errorMsg(const char* msg)  { textLine(4, true,  msg); }
    void clearMsg()                 { textLine(4, false, "");  }

    void binaries(
      size_t bootSize, const char* bootName,
      size_t appSize, const char* appName)
    {
      binaryLine(2, "boot", bootSize, bootName);
      binaryLine(3, "app", appSize, appName);
    }

    void progress(Burn phase, size_t done, size_t size) {
      const char* leadin;

      switch (phase) {
        case Burn::programming:   leadin = "burn";  break;
        case Burn::verifying:     leadin = "vrfy";  break;
        case Burn::complete:      leadin = "done";  done = 0;  break;
      }

      uint16_t x = static_cast<uint16_t>((done * 100 + 50) / size);

      display.fillRect( 0, 24, 128, 8, BLACK);
      display.fillRect(28, 24,   x, 8, WHITE);
      display.setTextColor(WHITE);
      display.setCursor(0, 24);
      display.print(leadin);
      display.display();
   }

  private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

    int lastButtonState;
    uint32_t buttonValidAt;

    void binaryLine(int line, const char* type, size_t size, const char* name) {
      char s[64];

      if (size > 0) snprintf(s, sizeof(s), "%3dk %s\n", sizeInK(size), name);
      else          snprintf(s, sizeof(s), "---  no %s binary\n", type);

      textLine(line, false, s);
    }

    void textLine(int line, bool invert, const char* msg) {
      int16_t y = 8 * (line - 1);

      display.fillRect(0, y, 128, 8, invert ? WHITE : BLACK);
      display.setTextColor(invert ? BLACK : WHITE);
      display.setCursor(invert ? 1 : 0, y);
      display.print(msg);
      display.display();
    }
  };

  OledFeatherwing oledFeatherwingInterface_;
}

Interface& oledFeatherwingInterface = oledFeatherwingInterface_;
