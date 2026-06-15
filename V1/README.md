# ScreenByte V1
A bare-metal handheld gaming console built on a naked ATmega328p - no Arduino board, no HAL abstraction, no Arduino based libraries. Every peripheral driven directly through registers in C.

## What It Is
ScreenByte is a from-scratch handheld console featuring a custom SSD1306 OLED graphics engine, hardware interrupt-driven input, and a pixel font renderer written entirely in bare-metal-c. Designed to be modular - future versions support SPI SD card game loading and swappable peripheral controllers on the handheld.

## Why It Exists
Built to demonstrate competency at low level working below Arduino's abstraction layer.
![V1 Schematic](V1_Schematic.pdf)

demo video: [https://youtu.be/sx_-BKFjsE0](https://youtu.be/sx_-BKFjsE0)
