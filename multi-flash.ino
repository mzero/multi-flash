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


#include <SPI.h>
#include <SdFat.h>

#include <Adafruit_DAP.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>

#include "interface.h"
#include "intf_serial.h"
#ifdef MF_OLED_FEATHERWING
  #include "intf_oldefeatherwing.h"
#endif
#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  #include "intf_circuitplayground.h"
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



InterfaceList interfaces({
  &serialInterface,
#ifdef MF_OLED_FEATHERWING
  &oledFetherwingInterface,
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




