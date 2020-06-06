#ifndef _INCLUDE_INTERFACE_H_
#define _INCLUDE_INTERFACE_H_

#include <cstddef>
#include <forward_list>
#include <initializer_list>


enum struct Event {
  idle,
  startFlash
};

enum struct Burn {
  programming,
  verifying,
  complete,
};

class Interface {
public:
  virtual void setup() = 0;
  virtual Event loop() = 0;

  virtual void startMsg(const char* msg) = 0;
  virtual void statusMsg(const char* msg) = 0;
  virtual void errorMsg(const char* msg) = 0;
  virtual void clearMsg() = 0;

  void startMsgf(const char* fmt, ... );
  void statusMsgf(const char* fmt, ... );
  void errorMsgf(const char* fmt, ... );

  virtual void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName)
    = 0;

  virtual void progress(Burn phase, size_t done, size_t size) = 0;
};



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */
// A Base implementation that does nothing

class InterfaceBase : public Interface {
public:
  void setup();
  Event loop();

  void startMsg(const char* msg);
  void statusMsg(const char* msg);
  void errorMsg(const char* msg);
  void clearMsg();

  void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName);

  void progress(Burn phase, size_t done, size_t size);
};



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */
// A List of Interfaces

class InterfaceList : public Interface {
  // Does not inherit from InterfaceBase so you won't forget to add methods
  // here when new methods are added to Interface
public:
  InterfaceList(std::initializer_list<Interface*> ifs);

  void setup();
  Event loop();

  void startMsg(const char* msg);
  void statusMsg(const char* msg);
  void errorMsg(const char* msg);
  void clearMsg();

  void binaries(
    size_t bootSize, const char* bootName,
    size_t appSize, const char* appName);

  void progress(Burn phase, size_t done, size_t size);

private:
  std::forward_list<Interface*> ifs;
};



/* -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- */
// Utilities

template< typename T >
T sizeInK(T s) { return (s + 1023) / 1024; }


#endif // _INCLUDE_INTERFACE_H_
