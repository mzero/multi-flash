#include "file_manager.h"

#include <delay.h>
#include <SPI.h>
#include <SdFat.h>

#include <Adafruit_DAP.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_SleepyDog.h>

namespace {

  // On-board external flash (QSPI or SPI) macros should already
  // defined in your board variant if supported
  // - EXTERNAL_FLASH_USE_QSPI
  // - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
  #if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;
  #elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(
      EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
  #else
    #error No QSPI/SPI flash are defined on your board variant.h !
  #endif

  Adafruit_SPIFlash flash(&flashTransport);

  FatFileSystem fatfs;

  Adafruit_USBD_MSC usb_msc;

  uint32_t changeSettledAt = 0;

  void noteFileSystemChange() {
    changeSettledAt = millis() + 250;
  }
}

namespace FileManager {

  bool changed() {
    if (changeSettledAt > 0 && changeSettledAt < millis()) {
      changeSettledAt = 0;
      return true;
    }
    return false;
  }

  bool changing() {
    return changeSettledAt > 0;
  }
}


/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

namespace {

  int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
  {
    return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
  }

  int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
  {
    return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
  }

  void msc_flush_cb (void)
  {
    flash.syncBlocks();
    fatfs.cacheClear();
    noteFileSystemChange();
  }

  bool setupMSC() {
    usb_msc.setID("e.k", "Multi-Flash", "1.0");
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
    usb_msc.setCapacity(flash.pageSize()*flash.numPages()/512, 512);
    usb_msc.setUnitReady(true);
    return usb_msc.begin();
  }

}

/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */
// Formatting via elm-chan's fatfs code

namespace elm_chan_fatfs {

  #include "elm-chan/ff.c"
  // This pulls in the required elm-chan fatfs code. It is done like this to
  // keep it isolated as much as possible.

  extern "C"
  {

    DSTATUS disk_status ( BYTE pdrv )
    {
      (void) pdrv;
      return 0;
    }

    DSTATUS disk_initialize ( BYTE pdrv )
    {
      (void) pdrv;
      return 0;
    }

    DRESULT disk_read (
      BYTE pdrv,    /* Physical drive nmuber to identify the drive */
      BYTE *buff,   /* Data buffer to store read data */
      DWORD sector, /* Start sector in LBA */
      UINT count    /* Number of sectors to read */
    )
    {
      (void) pdrv;
      return flash.readBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
    }

    DRESULT disk_write (
      BYTE pdrv,      /* Physical drive nmuber to identify the drive */
      const BYTE *buff, /* Data to be written */
      DWORD sector,   /* Start sector in LBA */
      UINT count      /* Number of sectors to write */
    )
    {
      (void) pdrv;
      return flash.writeBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
    }

    DRESULT disk_ioctl (
      BYTE pdrv,    /* Physical drive nmuber (0..) */
      BYTE cmd,   /* Control code */
      void *buff    /* Buffer to send/receive control data */
    )
    {
      (void) pdrv;

      switch ( cmd )
      {
        case CTRL_SYNC:
          flash.syncBlocks();
          return RES_OK;

        case GET_SECTOR_COUNT:
          *((DWORD*) buff) = flash.size()/512;
          return RES_OK;

        case GET_SECTOR_SIZE:
          *((WORD*) buff) = 512;
          return RES_OK;

        case GET_BLOCK_SIZE:
          *((DWORD*) buff) = 8;    // erase block size in units of sector size
          return RES_OK;

        default:
          return RES_PARERR;
      }
    }

  }

  bool checkFR(Interface& intf, const char* opstr, FRESULT r) {
    if (r != FR_OK) {
      intf.errorMsgf("%s (err %d)", opstr, r);
      return true;
    }
    intf.statusMsgf("%s good", opstr);
    return false;
  }

  bool format(Interface& intf) {

    FATFS elmchamFatfs;
    uint8_t workbuf[4096]; // Working buffer for f_fdisk function.

    if (checkFR(intf, "mkfs",
        f_mkfs("", FM_FAT | FM_SFD, 0, workbuf, sizeof(workbuf))))
      return false;

    if (checkFR(intf, "mount", f_mount(&elmchamFatfs, "", 1)))   return false;
    if (checkFR(intf, "label", f_setlabel("MultiFlash")))        return false;
    if (checkFR(intf, "unmount", f_unmount("")))                 return false;

    return true;
  }
}

namespace {
  bool checkSD(Interface& intf, const char* opstr, bool r) {
    if (!r)
      intf.errorMsgf("error in %s", opstr);
    else
      intf.statusMsgf("%s good", opstr);
    return !r;
  }

  bool touch(const char* path) {
    FatFile file;
    return file.open(fatfs.vwd(), path, FILE_WRITE);
  }
}
namespace FileManager {

  bool setup(Interface& intf) {
    if (!flash.begin()) {
      intf.errorMsg("Failed to initialize flash chip.");
      return false;
    }

    if (!fatfs.begin(&flash)) {
      intf.statusMsg("Formatting internal flash");

      if (!elm_chan_fatfs::format(intf))
        return false;

      // sync to make sure all data is written to flash
      flash.syncBlocks();

      if (!fatfs.begin(&flash)) {
        intf.statusMsg("Format failure");
        return false;
      }

      if (checkSD(intf, "mkdir", fatfs.mkdir("/.fseventsd")))         return false;
      if (checkSD(intf, "touch 1", touch("/.fseventsd/no_log")))      return false;
      if (checkSD(intf, "touch 2", touch("/.metadata_never_index")))  return false;
      if (checkSD(intf, "touch 3", touch("/.Trashes")))               return false;

      // sync to make sure all data is written to flash
      flash.syncBlocks();

      intf.statusMsg("Done, resetting....");
      Watchdog.enable(2000);
      delay(3000);
    }


    noteFileSystemChange();

    if (!setupMSC()) {
      intf.errorMsg("Failed to setup USB drive.");
      return false;
    }

    return true;
  }

  bool loop(Interface& intf) {
    return true;
  }
}

/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */

namespace {
  FatFile bootFile;
  FatFile appFile;


  bool matchBinFileName(const char* prefix, FatFile& file) {
    char name[512];
    file.getName(name, sizeof(name));

    String nameStr(name);
    nameStr.toLowerCase();

    return
      nameStr.startsWith(prefix)
      && nameStr.endsWith(".bin");
  }

}


FilesToFlash::FilesToFlash(Interface& intf) {
  intf.clearMsg();

  bootFile.close();
  appFile.close();

  FatFile root;
  if (!root.open("/")) {
    intf.errorMsg("open root failed");
  }

  FatFile file;
  while (file.openNext(&root, O_RDONLY)) {
    if (matchBinFileName("boot", file)) {
      if (!bootFile.isOpen()) {
        bootFile = file;
      } else {
        intf.errorMsg("multiple boot .bin files found");
      }
    } else if (matchBinFileName("app", file)) {
      if (!appFile.isOpen()) {
        appFile = file;
      } else {
        intf.errorMsg("multiple app .bin files found");
      }
    }
    file.close();
  }

  if (root.getError()) {
    intf.errorMsg("root openNext failed");
  }
  root.close();

  if (!bootFile.isOpen()) {
    intf.errorMsg("no boot .bin file found");
  }
}

FilesToFlash::~FilesToFlash() {
  bootFile.close();
  appFile.close();
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

void FilesToFlash::report(Interface& intf) {
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

  intf.binaries(bootSize, bootName, appSize, appName);
}
