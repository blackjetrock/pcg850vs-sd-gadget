# pcg850vs-sd-gadget
Gadget for the PC-G850VS family of pocket computers

This gadget:

1. Reads SD card files into memory and then sends them to the PCG850VS
2. Captures data from the PCG850VS and allows it to be written to the SD card
3. Can scope the PIO mod eoutputs of the PC-G850VS

The PCB uses a blue pill as a processor, and an SD card module for the SD card functions. An OLED display allows operatin away from a computer.

Limitations:

The data sent or received is stored in the RAM of the blue pill, so no files greater than the available RAM can be sent or received. An ARM chip with more RAM could be fitted to the blue pill.

Circuit
-------

The circuit uses a blue pill to do the processing. The PC-G850VS 11 pin interface is protected by series resistors. Power can come from either the blue pill or the PC-G850VS. If powered by the PC-G850VS short JP1. If you power the gadget from the blue pill when this is shorted then the 5V will be sent into the 5V of the 11 pin interface. I'm not sure what this will d, but it's probably not good.
There's an OLED display that is used to display menus. It is an I2C device. The SD card is held in a catalex SD card module, these are common on ebay.
Three keyswitches are used to control the menu system. As well as the OLED menus there are some commands that can be sent over the USB to serial connection. This needs a USB to serial adapter on J2.

The circuit involving Q1 is an inverter that should allow the UART functionality of the ARM processor to decode the serial data from the PC-G850VS. The current sketch does not use this but rather decodes the data stream using firmware.

For programming I use a STLINK V2 clone from ebay connected to the programming connector of the blue pill.


