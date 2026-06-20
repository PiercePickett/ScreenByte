# ScreenByte V1
A bare-metal handheld gaming console built on a naked ATmega328p - no Arduino board, no HAL abstraction, no Arduino based libraries. Every peripheral driven directly through registers in C.

## What It Is
ScreenByte is a from-scratch handheld console featuring a custom SSD1306 OLED graphics engine, hardware interrupt-driven input, and a pixel font renderer written entirely in bare-metal-c. Designed to be modular - future versions support SPI SD card game loading and swappable peripheral controllers on the handheld.

## Why It Exists
Built to demonstrate competency at low level working below Arduino's abstraction layer.
![V1 Schematic](V1_Schematic.pdf)

demo video: [https://youtu.be/sx_-BKFjsE0](https://youtu.be/sx_-BKFjsE0)

## How to Use

This project can be flashed using a dedicated AVR programmer (e.g. USBasp, AVRISP mkII) 
or using an Arduino Uno as an ISP programmer.

**If using an Arduino Uno as your programmer:**
You'll be working with two different pieces of Arduino software for two different 
roles:
- **ArduinoISP** (Examples > 11.ArduinoISP) gets uploaded *to the Uno itself*, turning 
  it into the programmer.
- **Arduino as ISP** is the *option you select* in Tools > Programmer when flashing 
  the target chip — it tells the IDE/avrdude to talk to your Uno instead of a 
  dedicated programmer.

This trips people up if they're new to bare-metal or haven't used an Arduino as a 
programmer before, since both have "ISP" in the name but do different jobs.

See [V1 Schematic](V1_Schematic.pdf) for the full wiring diagram between the Uno (programmer) and the 
target ATmega328P.

**Build from source:**
```bash
avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os -Wall -o main.elf main.c
avr-objcopy -O ihex -R .eeprom main.elf main.hex
```

**Flash to the target chip:**
```bash
avrdude -c stk500v1 -p m328p -P /dev/ttyACM0 -b 19200 -U flash:w:main.hex:i
```
*(Linux port shown `/dev/tty/ACM0` — Windows will use `COMx`)*
