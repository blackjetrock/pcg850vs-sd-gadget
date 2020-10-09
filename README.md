# pcg850vs-sd-gadget
Gadget for the PC-G850VS family of pocket computers

This gadget:

1. Reads SD card files into memory and then sends them to the PCG850VS
2. Captures data from the PCG850VS and allows it to be written to the SD card
3. Can scope the PIO mod eoutputs of the PC-G850VS

The PCB uses a blue pill as a processor, and an SD ard module for the SD card functions. An OLED display allows operatin away from a computer.

Limitations:

The data sent or received is stored in the RAM of the blue pill, so no files greater than the available RAM can be sent or recived. An ARM chip with more RAM could be fitted to the blue pill.
