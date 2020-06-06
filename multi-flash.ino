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

#include <Adafruit_DAP.h>

#include "config.h"     // must come before other project includes

#include "file_manager.h"

#include "interface.h"
#include "intf_serial.h"
#ifdef MF_OLED_FEATHERWING
  #include "intf_oledfeatherwing.h"
#endif
#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  #include "intf_circuitplayground.h"
#endif





/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */



InterfaceList interfaces({
  &serialInterface,
#ifdef MF_OLED_FEATHERWING
  &oledFeatherwingInterface,
#endif
#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  &circuitPlaygroundInterface,
#endif
});





/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */




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

void setup() {

  interfaces.setup();

  if (!FileManager::setup(interfaces))
    while(1) ;

  interfaces.startMsg("Multi-Flash");
}

void loop() {
  if (!FileManager::loop(interfaces)) {
    return;
  }

  if (FileManager::changed()) {
    FilesToFlash ftf(interfaces);
    ftf.report(interfaces);
  }

  switch (interfaces.loop()) {
    case Event::idle:
      break;

    case Event::startFlash: {

      if (FileManager::changing()) {
        interfaces.statusMsg("storage write in progress...");
        break; // don't flash if FS is still being written!
      }

      FilesToFlash ftf(interfaces);
      ftf.report(interfaces);
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




