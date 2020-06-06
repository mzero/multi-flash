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
//#define MF_OLED_FEATHERWING

// CONFIGURATION MACROS

#if 0  // enable these to define specific pins
  #define TARGET_SWDIO 10
  #define TARGET_SWCLK 11
  #define TARGET_SWRST 12
#endif

// default pin assignments per board
#ifdef ADAFRUIT_FEATHER_M0_EXPRESS
  #ifndef TARGET_SWDIO
    #define TARGET_SWDIO 10
    #define TARGET_SWCLK 11
    #define TARGET_SWRST 12
  #endif
#endif

#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  #ifndef TARGET_SWDIO
    #define TARGET_SWDIO 6    // labeled A1
    #define TARGET_SWCLK 9    // labeled A2
    #define TARGET_SWRST 10   // labeled A3
  #endif
#endif



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

#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
#include <Adafruit_CircuitPlayground.h>
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

enum struct Burn {
  programming,
  verifying,
  complete,
};

class Interface {
public:
  virtual void setup() { }
  virtual Event loop() { return Event::idle; }

  virtual void startMsg(const char* msg) { }
  virtual void statusMsg(const char* msg) { }
  virtual void errorMsg(const char* msg) { }
  virtual void clearMsg() { }

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

  virtual void progress(Burn phase, size_t done, size_t size) { }
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

private:
  Burn progressPhase = Burn::complete;
  int  progressPct = 0;

public:
  void progress(Burn phase, size_t done, size_t size) {
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

OledFeatherwing oledFetherwing;

#endif // MF_OLED_FEATHERWING


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0

class CircuitPlaygroundInterface : public Interface {
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

CircuitPlaygroundInterface circuitPlaygroundInterface;

#endif // ADAFRUIT_CIRCUITPLAYGROUND_M0


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

class InterfaceList : public Interface {
public:
  InterfaceList(std::initializer_list<Interface*> ifs) : ifs(ifs) { }

  void setup() { for (auto&& i : ifs) i->setup(); }
  Event loop() {
    for (auto&& i : ifs) {
      auto r = i->loop();
      if (r != Event::idle) return r;
    }
    return Event::idle;
  }

  void startMsg(const char* msg)  { for (auto&& i : ifs) i->startMsg(msg); }
  void statusMsg(const char* msg) { for (auto&& i : ifs) i->statusMsg(msg); }
  void errorMsg(const char* msg)  { for (auto&& i : ifs) i->errorMsg(msg); }
  void clearMsg()                 { for (auto&& i : ifs) i->clearMsg(); }

  void binaries(size_t bs, const char* bn, size_t as, const char* an)
    { for (auto&& i : ifs) i->binaries(bs, bn, as, an); }

  void progress(Burn phase, size_t done, size_t size)
    { for (auto&& i : ifs) i->progress(phase, done, size); }

private:
  std::forward_list<Interface*> ifs;
};

InterfaceList interfaces({
  &serialInterface,
#ifdef MF_OLED_FEATHERWING
  &oledFetherwing,
#endif
#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  &circuitPlaygroundInterface,
#endif
});





/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

struct FilesToFlash {
  FilesToFlash();
  ~FilesToFlash();

  FatFile bootFile;
  FatFile appFile;

  void report();

  bool okayToFlash();
  size_t imageSize();
  void rewind();
  int readNextBlock(uint8_t* buf, size_t blockSize);

private:
  static bool matchBinFileName(const char* prefix, FatFile& file);
  static void dumpBinFile(const char* type, FatFile& file);
};

FilesToFlash::FilesToFlash() {
  interfaces.clearMsg();

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

  if (!bootFile.isOpen()) {
    interfaces.errorMsg("no boot .bin file found");
  }
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

bool FilesToFlash::okayToFlash() {
  return bootFile.isOpen();
}

size_t FilesToFlash::imageSize() {
  return
    (bootFile.isOpen() ? bootFile.fileSize() : 0)
    + (appFile.isOpen() ? appFile.fileSize() : 0);
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

  auto imageSize = ftf.imageSize();
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
    interfaces.progress(Burn::programming, addr, imageSize);
    yield();
  } while (true);

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
      return false;
    }

    addr += sizeof(bufFile);  // must be in BUFSIZE chunks due to auto write
    interfaces.progress(Burn::verifying, addr, imageSize);
    yield();
  } while (true);

  interfaces.progress(Burn::complete, imageSize, imageSize);

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
bool mounted = false;

uint32_t changeSettledAt = 0;

void noteFileSystemChange() {
  changeSettledAt = millis() + 250;
}

bool fileSystemChanged() {
  if (changeSettledAt > 0 && changeSettledAt < millis()) {
    changeSettledAt = 0;
    return true;
  }
  return false;
}

bool fileSystemChanging() {
  return changeSettledAt > 0;
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

  noteFileSystemChange();

  digitalWrite(LED_BUILTIN, LOW);
}


void setupMSC() {
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("e.k", "Multi-Flash", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.pageSize()*flash.numPages()/512, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);

  usb_msc.begin();
}



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    interfaces.errorMsg("Failed to initialize flash chip!");
    while(1);
  }

  setupMSC();

  interfaces.setup();
  interfaces.startMsg("Multi-Flash");
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
    noteFileSystemChange();
  }

  if (fileSystemChanged()) {
    FilesToFlash ftf;
    ftf.report();
  }

  switch (interfaces.loop()) {
    case Event::idle:
      break;

    case Event::startFlash: {

      if (fileSystemChanging()) {
        interfaces.statusMsg("storage write in progress...");
        break; // don't flash if FS is still being written!
      }

      FilesToFlash ftf;
      ftf.report();
      if (!ftf.okayToFlash())
        break;

      interfaces.statusMsg("Starting flash...");
      delay(1000);

      FlashManager fm;
      fm.setup();
      if (fm.start())
        fm.program(ftf);
      fm.end();

      break;
    }

    default:
      interfaces.errorMsg("Event huh?");
  }
}




