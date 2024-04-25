# SD-1000
Sega flash multicart for SC-3000, SG-1000 and Mark III based on Pico Clone

SD-1000 is a multicart DIY yourself based on cheap "purple" Raspberry Pi Pico clone.

**WARNING!** "purple" Pico has not the same pinout of original Raspberry "green" ones, you MUST use the clone or you may damage your hardware.

![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega0.jpg)

Kicad project and gerbers files for the pcb are in the PCB folder, you need only a diode and a push buttons. The jumper for 5V to B3 must left open.
Add you pico clone, and flash the firmware SD1000_cart.uf2 in the Pico by connecting it while pressing button on Pico and drop it in the opened windows on PC.
After flashed with firmware, and every time you have to change your ROMS repository, you can simpli connect the Pico to PC and drag&drop "BIN","ROM","SG","SC" or "SMS" files into.

Even if the diode should protect your console, **DO NOT CONNECT PICO WHILE INSERTED IN A POWERED ON CONSOLE!**

![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega1.jpg)

## Credits
I have to thanks some friends for helping me in completing this projects in few time:

Thank to Robin Robin Edwards and his A8PicoCart (https://github.com/robinhedwards/A8PicoCart), i found very smart his way to manage the Flash RAM and the USB updates, so i admit i took large parte of his code for it!.
I used also the new and fantastic CVBasic for writing menu and file selector in Sega, thank to Oscar Toledo (https://nanochess.org/) for creating it and supporting me in an issue with SC-3000.
And, last but not least, a special thank to my friend Fabio (https://github.com/fabiodl) that from Japan leaded me out of a couple of tricky steps.


![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega2.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega3.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega4.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega5.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega6.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/SD-1000/main/Pictures/sega7.jpg)

## To DO
A 3D printed cover... i'm working on it and i'll publish here when ready


