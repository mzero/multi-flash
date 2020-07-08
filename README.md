# Multi-Flash

Multi-Flash is a convienient flash programmer for SAMD2x based
devices that works on any Arduino compatible board with an external flash
chip. It works like this:

1. Plug a device running Multi-Flash (the programmer) into your computer.
   It will mount as a small USB drive.

2. Copy over the .bin files for the bootloader and application you want to
   flash onto the target device.

   Name the bootloader file  `boot`_something_`.bin`
   Name the application file `app`_something_`.bin`

   The _something_ bits don't matter, and don't have to match. Just make sure
   there is at most one file each of boot... and app...

3. Disconnect the programmer from computer now, if you want.

4. Connect the target device to the programmer

5. Press the "A" button on the programmer to start the flashing

5. Repeat steps 4 & 5 to flash as many as you need!

## Targets

This programmer uses the Adafruit_DAP library to program targets over SWD. The
ATSAMD ARM based processors have this. It is a simple interface with just three
connections between target and programmer:

* SWDIO
* SWCLK
* RESET (active low)

For many boards, these are available as small solder pads somewhere. If you
are building a board with a processor, you'll need to be sure these signals are
made available for connection.

The code currently will only work with SAMD2x processors. It will probably be
trival to adapt for SAMD5x boards, as the Adafruit_DAP library supports them.
However, there are some code changes needed: the bootloader size is different,
and a different class from the library must be used. You can make these
changes in your copy, but some work is needed so the code can accomodate both
gracefully.

## Programmers

The code here should work on any board, SAMD or not.... So long as the board
has an external flash memory chip, and supports the TinyUSB library. These are
needed to implement the USB file system.

This code has been successfully build and run on:

* Adafruit Feather M0 Express + OLED Featherwing
* Adafruit Circuit Playground Express

See the `hardware` folder for images of how to set these up.

The OLED is optional, but if you have it, the code provides better feedback
and progress bars during flashing.

## Connecting

Which pins on the programmer are connected to the target's lines can be
configured in `config.h`. That file has these defaults for these programmer
devices:

### Feather M0 Express
* SWDIO 10
* SWCLK 11
* RESET 12

### Circuit Playground Express
* SWDIO 6    — labeled A1
* SWCLK 9    — labeled A2
* SWRST 10   — labeled A3

You must also connect `GND` between the target and the programmer.

You can power the target via its normal power source - or you can try powering
it from the 3.3V source on the programmer. Generally, the 3.3V pin on
boards isn't designed a whole other board... but during flashing, the target
isn't using that much power. Nonetheless, it make cause pretty big voltage
droops on connect and disconnect. I use 2 x 100uF caps between 3.3V and GND
right where the target connects.

## Libraries

This code uses the following libraries, and they'll need to be installed
in your Arduino IDE:

* Adafruit_BusIO
* Adafruit_DAP -- see NOTE below
* Adafruit_SleepyDog_Library
* Adafruit_SPIFlash
* Adafruit_TinyUSB_Library
* SdFat_-\_Adafruit_Fork

Also, if using an OLED Featherwing:

* Adafruit-GFX-Library
* Adafruit_SSD1306

Also, if compiling for a Circuit Playground:

* Adafruit_Circuit_Playground

NOTE: You'll need the `userrow` branch in my for of Adafruit_DAP, found here:
	https://github.com/mzero/Adafruit_DAP/tree/userrow


## Credits

This is a mashup of the following example sketches:

* Adafruit_DAP / flash_from_SPI_flash
* Adafruit_SPIFlash / SdFat_format
* Adafruit_SPIFlash / SdFat_ReadWRite
* Adafruit_TinyUSB / msc_external_flash

In order to format the FatFS on the flash drive like SdFat_format does, this
code also contains a copy of some of elm-chan's FatFs source. It is in the
`elm-chan` directory and retains its copyright and license.


