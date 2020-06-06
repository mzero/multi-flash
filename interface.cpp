#include "interface.h"

#include <cstdarg>
#include <cstdio>


void Interface::startMsgf(const char* fmt, ... ) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  startMsg(buf);
}

void Interface::statusMsgf(const char* fmt, ... ) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  statusMsg(buf);
}

void Interface::errorMsgf(const char* fmt, ... ) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  errorMsg(buf);
}



void InterfaceBase::setup() { }
Event InterfaceBase::loop() { return Event::idle; }

void InterfaceBase::startMsg(const char* msg) { }
void InterfaceBase::statusMsg(const char* msg) { }
void InterfaceBase::errorMsg(const char* msg) { }
void InterfaceBase::clearMsg() { }

void InterfaceBase::binaries(
  size_t bootSize, const char* bootName,
  size_t appSize, const char* appName)
  { }

void InterfaceBase::progress(Burn phase, size_t done, size_t size) { }




InterfaceList::InterfaceList(std::initializer_list<Interface*> ifs) : ifs(ifs) { }

void InterfaceList::setup() { for (auto&& i : ifs) i->setup(); }
Event InterfaceList::loop() {
  for (auto&& i : ifs) {
    auto r = i->loop();
    if (r != Event::idle) return r;
  }
  return Event::idle;
}

void InterfaceList::startMsg(const char* msg)  { for (auto&& i : ifs) i->startMsg(msg); }
void InterfaceList::statusMsg(const char* msg) { for (auto&& i : ifs) i->statusMsg(msg); }
void InterfaceList::errorMsg(const char* msg)  { for (auto&& i : ifs) i->errorMsg(msg); }
void InterfaceList::clearMsg()                 { for (auto&& i : ifs) i->clearMsg(); }

void InterfaceList::binaries(size_t bs, const char* bn, size_t as, const char* an)
  { for (auto&& i : ifs) i->binaries(bs, bn, as, an); }

void InterfaceList::progress(Burn phase, size_t done, size_t size)
  { for (auto&& i : ifs) i->progress(phase, done, size); }

