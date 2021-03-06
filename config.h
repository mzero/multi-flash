#ifndef _CONFIG_H_
#define _CONFIG_H_

// FEATURE MACROS

// #define MF_WAIT_FOR_SERIAL
  // This is useful for debugging this code, or if that is your only
  // interface to the programmer.

#define MF_OLED_FEATHERWING
  // Comment out if you don't have such a display.

// CONFIGURATION MACROS

#if 0  // enable these to define specific pins
  #define TARGET_SWDIO 10
  #define TARGET_SWCLK 11
  #define TARGET_SWRST 12
#endif

// default pin assignments per board
#if defined(ADAFRUIT_FEATHER_M0_EXPRESS) || defined(ADAFRUIT_FEATHER_M4_EXPRESS)
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


#endif // _CONFIG_H_
