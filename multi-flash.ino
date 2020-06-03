/*

This is a mashup of the following example sketches:

  Adafruit_DAP / flash_from_SPI_flash
  Adafruit_SPIFlash / SdFat_format, SdFat_ReadWRite
  Adafruit_TinyUSB / msc_external_flash

Multi-Flash is a convienient flash programmer for SAMD21 and SAMD51 based
devices. It works like this:

1. Plug a device running Multi-Flash (the programmer) into your computer.
   It will mount as a small USB drive.

2. Copy over the .bin files for the bootloader and/or application you want to
   flash onto the target device.

   Name the bootloader file  boot<something>.bin
   Name the application file app<something>.bin

   The <something> bits don't matter, and don't have to match. Just make sure
   there is at most one file each of boot... and app...

3. Disconnect the programmer from computer now, if you want.

4. Connect the target device

5. Press the button on the programmer to start the flashing

5. Repeat steps 4 & 5 to flash as many as you need!


*/

/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

// FEATURE MACROS

#define MF_WAIT_FOR_SERIAL
#define MF_OLED_FEATHERWING


// CONFIGURAATION MACROS

#define TARGET_SWDIO 10
#define TARGET_SWCLK 11
#define TARGET_SWRST 12




/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

#include <forward_list>
#include <initializer_list>

#include <SPI.h>
#include <SdFat.h>

#include <Adafruit_DAP.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>

#ifdef MF_OLED_FEATHERWING
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif


// On-board external flash (QSPI or SPI) macros should already
// defined in your board variant if supported
// - EXTERNAL_FLASH_USE_QSPI
// - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
#if defined(EXTERNAL_FLASH_USE_QSPI)
  Adafruit_FlashTransport_QSPI flashTransport;

#elif defined(EXTERNAL_FLASH_USE_SPI)
  Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);

#else
  #error No QSPI/SPI flash are defined on your board variant.h !
#endif

Adafruit_SPIFlash flash(&flashTransport);


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

template< typename T >
T sizeInK(T s) { return (s + 1023) / 1024; }


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

enum struct Event {
  idle,
  startFlash
};

class Interface {
public:
  virtual void setup() { }
  virtual Event loop() { return Event::idle; }

  virtual void startMsg(const char* msg) { }
  virtual void statusMsg(const char* msg) { }
  virtual void errorMsg(const char* msg) { }

  void startMsgf(const char* fmt, ... ) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    startMsg(buf);
  }

  void statusMsgf(const char* fmt, ... ) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    statusMsg(buf);
  }

  void errorMsgf(const char* fmt, ... ) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    errorMsg(buf);
  }


  virtual void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName)
    { }
};


class SerialInterface : public Interface {
public:
  void setup() {
    Serial.begin(115200);
#ifdef MF_WAIT_FOR_SERIAL
    while (!Serial) {
      delay(100);
    }
#endif
  }

  void startMsg(const char* msg) {
    Serial.println();
    Serial.println();
    Serial.println(msg);
    for (auto i = strlen(msg); i; --i)
      Serial.print('-');
    Serial.println();
  }

  void statusMsg(const char* msg) {
    Serial.println(msg);
  }

  void errorMsg(const char* msg) {
    Serial.print("** ");
    Serial.println(msg);
  }

  void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName)
  {
    if (bootSize > 0) Serial.printf("> boot: %3dk %s\n", sizeInK(bootSize), bootName);
    else              Serial.printf("> boot: ---  no binary\n");

    if (appSize > 0)  Serial.printf("> app:  %3dk %s\n", sizeInK(appSize), appName);
    else              Serial.printf("> app:  ---  no binary\n");
  }
};

SerialInterface serialInterface;


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

// FEATURE HARDWARE

#ifdef MF_OLED_FEATHERWING


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



class OledFeatherwing : public Interface {
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

  void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName)
  {
    binaryLine(2, "boot", bootSize, bootName);
    binaryLine(3, "app", appSize, appName);
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

OledFeatherwing oledFetherwing;

#endif // MF_OLED_FEATHERWING



class InterfaceList : public Interface {
public:
  InterfaceList(std::initializer_list<Interface*> ifs) : ifs(ifs) { }

  void setup() { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->setup(); }
  Event loop() {
    for (auto i = ifs.begin(); i != ifs.end(); ++i) {
      auto r = (*i)->loop();
      if (r != Event::idle) return r;
    }
    return Event::idle;
  }

  void startMsg(const char* msg)  { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->startMsg(msg); }
  void statusMsg(const char* msg) { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->statusMsg(msg); }
  void errorMsg(const char* msg)  { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->errorMsg(msg); }

  void binaries(size_t bs, const char* bn, size_t as, const char* an)
    { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->binaries(bs, bn, as, an); }

private:
  std::forward_list<Interface*> ifs;
};

InterfaceList interfaces({
  &serialInterface,
#ifdef MF_OLED_FEATHERWING
  &oledFetherwing,
#endif
});





/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

struct FilesToFlash {
  FilesToFlash();
  ~FilesToFlash();

  FatFile bootFile;
  FatFile appFile;

  void report();

  void rewind();
  int readNextBlock(uint8_t* buf, size_t blockSize);

private:
  static bool matchBinFileName(const char* prefix, FatFile& file);
  static void dumpBinFile(const char* type, FatFile& file);
};

FilesToFlash::FilesToFlash() {
  FatFile root;
  if (!root.open("/")) {
    interfaces.errorMsg("open root failed");
  }

  FatFile file;
  while (file.openNext(&root, O_RDONLY)) {
    if (matchBinFileName("boot", file)) {
      if (!bootFile.isOpen()) {
        bootFile = file;
      } else {
        interfaces.errorMsg("multiple boot .bin files found");
      }
    } else if (matchBinFileName("app", file)) {
      if (!appFile.isOpen()) {
        appFile = file;
      } else {
        interfaces.errorMsg("multiple app .bin files found");
      }
    }
    file.close();
  }

  if (root.getError()) {
    interfaces.errorMsg("root openNext failed");
  }
  root.close();
}

FilesToFlash::~FilesToFlash() {
  bootFile.close();
  appFile.close();
}

bool FilesToFlash::matchBinFileName(const char* prefix, FatFile& file) {
  char name[512];
  file.getName(name, sizeof(name));

  String nameStr(name);
  nameStr.toLowerCase();

  return
    nameStr.startsWith(prefix)
    && nameStr.endsWith(".bin");
}

void FilesToFlash::rewind() {
  if (bootFile.isOpen())  bootFile.rewind();
  if (appFile.isOpen()) appFile.rewind();
}

int FilesToFlash::readNextBlock(uint8_t* buf, size_t bufsize) {
  if (bootFile.isOpen()) {
    auto r = bootFile.read(buf, bufsize);
    if (r < 0) return r;
    if (r == bufsize) return r;

    if (appFile.isOpen()) {
      buf += r;
      bufsize -= r;

      auto s = appFile.read(buf, bufsize);
      if (s < 0) return s;

      r += s;
    }

    return r;
  }
  return 0;
}

void FilesToFlash::report() {
  size_t bootSize = 0;
  size_t appSize = 0;

  char bootName[512];
  char appName[512];

  if (bootFile.isOpen()) {
      bootSize = bootFile.fileSize();
      bootFile.getName(bootName, sizeof(bootName));
  }

  if (appFile.isOpen()) {
      appSize = appFile.fileSize();
      appFile.getName(appName, sizeof(appName));
  }

  interfaces.binaries(bootSize, bootName, appSize, appName);
}



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

class FlashManager {

public:
  void setup();
  bool start();
  bool program(FilesToFlash& ftf);
  void end();

private:
  //create a DAP for programming Atmel SAM devices
  Adafruit_DAP_SAM dap;

  bool dap_error();
  static void error(const char* text);
};

void FlashManager::setup() {
  dap.begin(TARGET_SWCLK, TARGET_SWDIO, TARGET_SWRST, &error);
}

bool FlashManager::start() {
  bool doAgain = false;
  do {
    if (! dap.dap_disconnect())                     return dap_error();
    if (! dap.dap_connect())                        return dap_error();
    if (! dap.dap_transfer_configure(0, 128, 128))  return dap_error();
    if (! dap.dap_swd_configure(0))                 return dap_error();
    if (! dap.dap_reset_link())                     return dap_error();
    if (! dap.dap_swj_clock(50))                    return dap_error();
    if (! dap.dap_reset_target_hw())                return dap_error();
    if (! dap.dap_reset_link())                     return dap_error();
    if (! dap.dap_target_prepare())                 return dap_error();

    uint32_t dsu_did;
    if (! dap.select(&dsu_did)) {
      interfaces.errorMsgf("Unknown device 0x%x", dsu_did);
      return false;
    }
    interfaces.statusMsgf(
      "->%s, %dk", dap.target_device.name, sizeInK(dap.target_device.flash_size));

    dap.fuseRead(); // fuse operations don't return a result (!)
    if (dap._USER_ROW.BOOTPROT != 7 || dap._USER_ROW.LOCK != 0xffff) {
      if (doAgain) {
        interfaces.errorMsgf("unprotection failed");
        return false;
      }

      dap._USER_ROW.BOOTPROT = 7;   // unprotect the boot area
      dap._USER_ROW.LOCK = 0xffff;  // unprotect all the regions
      dap.fuseWrite();
      doAgain = true;
      interfaces.statusMsgf("NVRAM now unprotected, resetting");
    } else {
      doAgain = false;
    }
  } while (doAgain);

  return true;
}

bool FlashManager::program(FilesToFlash& ftf) {
#define BUFSIZE 256       //don't change!
  uint8_t bufFile[BUFSIZE];
  uint8_t bufFlash[BUFSIZE];

  dap.erase();

  auto startAddr = dap.program_start();

  auto addr = startAddr;
  ftf.rewind();
  do {
    auto r = ftf.readNextBlock(bufFile, sizeof(bufFile));
    if (r < 0) {
      interfaces.errorMsg("error reading binaries");
      return false;
    }
    if (r == 0)
      break;
    dap.programBlock(addr, bufFile);

    addr += sizeof(bufFile);  // must be in BUFSIZE chunks due to auto write
    Serial.print('.');
  } while (true);
  Serial.println();

  addr = startAddr;
  ftf.rewind();
  do {
    auto r = ftf.readNextBlock(bufFile, sizeof(bufFile));
    if (r < 0) {
      interfaces.errorMsg("error reading binaries");
      return false;
    }
    if (r == 0)
      break;
    dap.readBlock(addr, bufFlash);

    if (memcmp(bufFile, bufFlash, r) != 0) {
      interfaces.errorMsgf("mismatch @%08x", addr);

      auto f = &bufFile[0];
      auto g = &bufFlash[0];
      for (int i = 0; i < 8; ++i) {
        Serial.printf("f: %02x %02x %02x %02x   %02x %02x %02x %02x\n", *f++, *f++, *f++, *f++,   *f++, *f++, *f++, *f++);
        Serial.printf("g: %02x %02x %02x %02x   %02x %02x %02x %02x\n", *g++, *g++, *g++, *g++,   *g++, *g++, *g++, *g++);
        Serial.println();
      }

      return false;
    }

    addr += sizeof(bufFile);  // must be in BUFSIZE chunks due to auto write
    Serial.print('+');
  } while (true);
  Serial.println();

  return true;
}


void FlashManager::end() {
  dap.dap_set_clock(50);
  dap.deselect();
  dap.dap_disconnect();   // errors in this case are irrelevant
}


bool FlashManager::dap_error() {
  interfaces.errorMsg(dap.error_message);
  return false;
}

void FlashManager::error(const char* text) {
  // should handle the absurd ")" case
  interfaces.errorMsgf("DAP error: %s", text);
}


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */


// file system object from SdFat
FatFileSystem fatfs;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Set to true when PC write to flash
bool mounted;
bool changed;


void setup() {

  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    interfaces.errorMsg("Failed to initialize flash chip!");
    while(1);
  }

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("e.k", "Multi-Flash", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.pageSize()*flash.numPages()/512, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);

  usb_msc.begin();


  interfaces.setup();
  interfaces.startMsg("Multi-Flash");

  mounted = false;
  changed = false;

}

void loop() {
  if (!mounted) {
    // First call begin to mount the filesystem.  Check that it returns true
    // to make sure the filesystem was mounted.
    if (!fatfs.begin(&flash)) {
      interfaces.errorMsg("Failed to mount filesystem!");
      delay(2000);
      return;
    }
    mounted = true;
    changed = true;
  }

  if (changed) {
    changed = false;

    FilesToFlash ftf;
    ftf.report();

    delay(1000);
  }

  switch (interfaces.loop()) {
    case Event::idle:
      break;

    case Event::startFlash: {
      interfaces.statusMsg("Starting flash...");
      delay(1000);

      FilesToFlash ftf;
      ftf.report();

      FlashManager fm;
      fm.setup();
      if (fm.start())
        if (fm.program(ftf))
          interfaces.statusMsg("done");

      fm.end();

      break;
    }

    default:
      interfaces.errorMsg("Event huh?");
  }
}





// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  // sync with flash
  flash.syncBlocks();

  // clear file system's cache to force refresh
  fatfs.cacheClear();

  changed = true;

  digitalWrite(LED_BUILTIN, LOW);
}




