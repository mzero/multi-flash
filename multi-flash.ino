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

