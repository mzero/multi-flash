#ifndef _FLASH_MANAGER_H_
#define _FLASH_MANAGER_H_

#include "file_manager.h"
#include "interface.h"

namespace FlashManager {

  bool program(Interface&, FilesToFlash&);

}

#endif // _FLASH_MANAGER_H_
