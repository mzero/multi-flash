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

#include "config.h"     // must come before other project includes

#include "file_manager.h"
#include "flash_manager.h"

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

      FlashManager::program(interfaces, ftf);

      break;
    }

    default:
      interfaces.errorMsg("Event huh?");
  }
}

