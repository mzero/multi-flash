#include "flash_manager.h"

#include <Adafruit_DAP.h>

#include "config.h"


namespace {

  class Flasher {
    public:
      Flasher(Interface& intf);
      ~Flasher();

      bool start();
      bool program(FilesToFlash& ftf);

    private:
      Interface& intf;

      Adafruit_DAP_SAM dap;

      static Flasher* current;

      bool dap_error();
      static void error(const char* text);
  };

  Flasher* Flasher::current = NULL;

  Flasher::Flasher(Interface& intf) : intf(intf) {
    current = this;

    dap.begin(TARGET_SWCLK, TARGET_SWDIO, TARGET_SWRST, &error);
  }

  Flasher::~Flasher() {
    if (current == this)
      current = NULL;
       // cleared first, as we don't report errors at this point

    dap.dap_set_clock(50);
    dap.deselect();
    dap.dap_disconnect();
  }

  bool Flasher::start() {
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
        if (dsu_did == 0)
          intf.errorMsg("No target device connected");
        else
          intf.errorMsgf("Unknown device 0x%x", dsu_did);
        return false;
      }
      intf.statusMsgf(
        "->%s, %dk", dap.target_device.name, sizeInK(dap.target_device.flash_size));

      dap.fuseRead(); // fuse operations don't return a result (!)
      if (dap._USER_ROW.BOOTPROT != 7 || dap._USER_ROW.LOCK != 0xffff) {
        if (doAgain) {
          intf.errorMsgf("unprotection failed");
          return false;
        }

        dap._USER_ROW.BOOTPROT = 7;   // unprotect the boot area
        dap._USER_ROW.LOCK = 0xffff;  // unprotect all the regions
        dap.fuseWrite();
        doAgain = true;
        intf.statusMsgf("NVRAM now unprotected, resetting");
      } else {
        doAgain = false;
      }
    } while (doAgain);

    return true;
  }

  bool Flasher::program(FilesToFlash& ftf) {
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
        intf.errorMsg("error reading binaries");
        return false;
      }
      if (r == 0)
        break;
      dap.programBlock(addr, bufFile);

      addr += sizeof(bufFile);  // must be in BUFSIZE chunks due to auto write
      intf.progress(Burn::programming, addr, imageSize);
      yield();
    } while (true);

    addr = startAddr;
    ftf.rewind();
    do {
      auto r = ftf.readNextBlock(bufFile, sizeof(bufFile));
      if (r < 0) {
        intf.errorMsg("error reading binaries");
        return false;
      }
      if (r == 0)
        break;
      dap.readBlock(addr, bufFlash);

      if (memcmp(bufFile, bufFlash, r) != 0) {
        intf.errorMsgf("mismatch @%08x", addr);
        return false;
      }

      addr += sizeof(bufFile);  // must be in BUFSIZE chunks due to auto write
      intf.progress(Burn::verifying, addr, imageSize);
      yield();
    } while (true);

    intf.progress(Burn::complete, imageSize, imageSize);

    return true;
  }


  bool Flasher::dap_error() {
    intf.errorMsg(dap.error_message);
    return false;
  }

  void Flasher::error(const char* text) {
    if (strcmp(text, ")") == 0) {
      // The DAP library prints some error messages directly to Serial, except
      // for the closing ')' which it prints by calling the error function.
      Serial.println(text); // clean up the Serial output
      text = "invalid response";
    }
    if (current) {
      // should handle the absurd ")" case
      current->intf.errorMsgf("DAP error: %s", text);
    }
  }

}

namespace FlashManager {

  bool program(Interface& intf, FilesToFlash& ftf) {
    Flasher flasher(intf);
    return flasher.start() && flasher.program(ftf);
  }

}
