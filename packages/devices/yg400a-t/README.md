# YG400A-T Smoke alarm

This device has a T1-U-HL Wifi MCU (Beken BK7238) and a BA45F5220 (marked BA50F1V) smoke detector MCU.

The BA45F5220 controls the LED, test button, piezo electric alarm, and blue LED. When the optical smoke sensor detects smoke, it will put a 3V signal on P15 (pin 21) of the BK7238.

[Some more info and a crude schematic can be found here](https://www.elektroda.com/rtvforum/topic3941698-60.html).

## Initial flashing

There are UART pins exposed on the PCB, and theres enough room in the casing to allow a pin header to be soldered here. I disconnected the batteries and provided 3.3V through the header.

OTA updates do not work currently.
