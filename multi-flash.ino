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



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

#include <forward_list>
#include <initializer_list>

#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>

#ifdef MF_OLED_FEATHERWING
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
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

class Interface : public Print {
public:
  virtual void setup() { }
  virtual void loop() { }

  virtual size_t write(uint8_t) { };
  using Print::write;

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

  size_t write(uint8_t c) { return Serial.write(c); }
};

SerialInterface serialInterface;


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

// FEATURE HARDWARE

#ifdef MF_OLED_FEATHERWING

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
    newLine = true;
  }

  size_t write(uint8_t c) {
    if (newLine) {
      display.fillRect(0, 24, 128, 8, BLACK);
      display.setCursor(0, 24);
      newLine = false;
    }
    if (c == '\n') {
      display.display();
      newLine = true;
      return 1;
    } else {
      return display.write(c);
    }
  }

private:
  Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
  bool newLine = false;
};

OledFeatherwing oledFetherwing;

#endif // MF_OLED_FEATHERWING



class InterfaceList : public Interface {
public:
  InterfaceList(std::initializer_list<Interface*> ifs) : ifs(ifs) { }

  void setup() { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->setup(); }
  void loop() { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->loop(); }

  size_t write(uint8_t c) { for (auto i = ifs.begin(); i != ifs.end(); ++i) (*i)->write(c); };
  using Print::write;

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

  void dump();

private:
  static bool matchBinFileName(const char* prefix, FatFile& file);
  static void dumpBinFile(const char* type, FatFile& file);
};

FilesToFlash::FilesToFlash() {
  FatFile root;
  if (!root.open("/")) {
    interfaces.println("open root failed");
  }

  FatFile file;
  while (file.openNext(&root, O_RDONLY)) {
    if (matchBinFileName("boot", file)) {
      if (!bootFile.isOpen()) {
        bootFile = file;
      } else {
        interfaces.println("multiple boot .bin files found");
      }
    } else if (matchBinFileName("app", file)) {
      if (!appFile.isOpen()) {
        appFile = file;
      } else {
        interfaces.println("multiple app .bin files found");
      }
    }
    file.close();
  }

  if (root.getError()) {
    interfaces.println("root openNext failed");
  }
  root.close();
}

FilesToFlash::~FilesToFlash() {
  bootFile.close();
  appFile.close();
}

void FilesToFlash::dump() {
  dumpBinFile("boot", bootFile);
  dumpBinFile("app", appFile);
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

void FilesToFlash::dumpBinFile(const char* type, FatFile& file) {
  if (file.isOpen()) {
    file.printFileSize(&interfaces);
    interfaces.write(' ');
    file.printName(&interfaces);
    interfaces.println();
  } else {
    interfaces.print("        -- no "); // printFileSize outputs 10 characters
    interfaces.print(type);
    interfaces.println(" binary");
  }
}










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
    interfaces.println("Error, failed to initialize flash chip!");
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
  interfaces.println("Multi-Flash");

  mounted = false;
  changed = false;

}

void loop() {
  interfaces.loop();

  if (!mounted) {
    // First call begin to mount the filesystem.  Check that it returns true
    // to make sure the filesystem was mounted.
    if (!fatfs.begin(&flash)) {
      interfaces.println("Failed to mount filesystem!");
      interfaces.println("Will wait a bit for host to init it!");
      delay(2000);
      return;
    }
    interfaces.println("Mounted filesystem!");
    mounted = true;
    changed = true;
  }

  if (changed) {
    changed = false;

    FilesToFlash ftf;
    ftf.dump();

    delay(1000);
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




