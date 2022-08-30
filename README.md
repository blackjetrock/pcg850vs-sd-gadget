# pcg850vs-sd-gadget
Gadget for the PC-G850VS family of pocket computers

This gadget:

1. Reads SD card files into memory and then sends them to the PCG850VS
2. Captures data from the PCG850VS and allows it to be written to the SD card
3. Can scope the PIO mod eoutputs of the PC-G850VS

The PCB uses a blue pill as a processor, and an SD card module for the SD card functions. An OLED display allows operatin away from a computer.

Limitations:

The data sent or received is stored in the RAM of the blue pill, so no files greater than the available RAM can be sent or received. 
The SIO protocol has handshaking so this could be used to pause data from the PC-G850VS to the gadget when writing to the SD card. Reading from the card 
could be done without a problem as data is sent once it is read.

An ARM chip with more RAM could be fitted to the blue pill, this is more involved as it requires desoldering the QFP and putting a new one on.

An issue with early code meant that the SD card may not initialise properly. A code fix for this has bee pushed to the repository.

File Tags
---------

If the gadget detects a tag of '@file' (case sensitive) in the file that it has read from the G850 then it usd the filename specified after the tag
(delimited with a space) as the name to use if the file is written to the SD card. If the file tag does not exist then the gadget uses unique
filenames of the form PCGnnnn.DAT where nnnn is chosen to be the next unused value on the SD card.

Circuit
-------

The circuit uses a blue pill to do the processing. The PC-G850VS 11 pin interface is protected by series resistors. Power can come from either the blue pill or the PC-G850VS. If powered by the PC-G850VS short JP1. If you power the gadget from the blue pill when this is shorted then the 5V will be sent into the 5V of the 11 pin interface. I'm not sure what this will d, but it's probably not good.
There's an OLED display that is used to display menus. It is an I2C device. The SD card is held in a catalex SD card module, these are common on ebay.
Three keyswitches are used to control the menu system. As well as the OLED menus there are some commands that can be sent over the USB to serial connection. This needs a USB to serial adapter on J2.

The circuit involving Q1 is an inverter that should allow the UART functionality of the ARM processor to decode the serial data from the PC-G850VS. The current sketch does not use this but rather decodes the data stream using firmware.

For programming I use a STLINK V2 clone from ebay connected to the programming connector of the blue pill.

Construction Notes
==================

Components are fitted as shown on the schematic, apart from a few noted here:

The SD card reader module should be powered from 5V, not 3V3. The PCBs all have a 3V3 supply wired to the module. I have found that some modules will work with a 3V3 supply, but some won't. Cutting the track to the power pin of the module and running  wire from 3V3 to the supply pin of the module fixes the problem.

R14, Q1 and R15 are a hardware level shifter that takes the transmitted data (TX) from the PC-G850VS and sends it to the hardware USART on the blue pill. I don't use this at the moment as the code can decode the serial data. These components can be omitted or fitted. The sketch will have to be adjusted if you want to use the hardware USART.

J7 is a power jumper. If it is shorted then power comes from the PC-G850VS. If open then the blue pill can power the circuit (useful if programming the blue pill).
R4 and R5 are power connections and should be 0R links or wire links.

The blue pill, display and SD card reader can either be soldered to the PCB or fitted in sockets. If soldered then th eprofile is much lower but obviously these parts are harder to remve if you need to.

The SD card reader comes with a right angle type connector, for fitting on the PCB I desolder the connector it comes with and fit a straight connector.

I use a straight pin header for J1 and solder it on the top of the board. This is because straight headers are cheaper and they also lead to a lower profile once fitted.

OLED displays come in two pinouts, with reversed power connections. There's two connectors for the OLED display, one for each pinout. Make sure the OLED display is connected to the correct pinout connector.

I use an ST-LINKV2 from ebay for programming the blue pill. Any method of getting the sketch on to the blue pill should be fine.

Code
----

The menu structure that the code uses varies with the version of code that is running and th efunctions that are added to the code.
The menu option names have been elaborated from th ebasic names given in the instruction sheet, but it should be possible to work out 
which option does what.


Supported Devices
-----------------

The gadget has been used on the:
PC-G850VS
PC-G850

