#ifndef _FILE_MANAGER_H_
#define _FILE_MANAGER_H_

#include <cstddef>
#include <cstdint>

#include "interface.h"


namespace FileManager {
  bool setup(Interface&);
  bool loop(Interface&);

  bool changed();
  bool changing();
}

struct FilesToFlash {
  FilesToFlash(Interface&);
  ~FilesToFlash();

  bool okayToFlash();

  void report(Interface&);

  size_t imageSize();
  void rewind();
  int readNextBlock(uint8_t* buf, size_t blockSize);
};



#endif // _FILE_MANAGER_H_
