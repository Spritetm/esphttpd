This is a branch of esphttpd that can serve files from FAT-formatted SD-cards. It uses
a GPIO-based SPI driver to access the SD-card in FAT mode (hardware SPI also is 
implemented but very flaky) and Elm-chans FatFS to read the filesystem.

The SD-card is connected to the following I/O pins. Both normal-sized SD as well as micro-SD
as well as SDHC cards are supported, although not all cards seem to implement the SPI protocol
in the way I expect them to - a few of my cards just won't work. Also, because I use bitbanging, 
everything is gonna be very slow - expect speeds of about 20K/second.
(see eg http://elasticsheep.com/wp-content/uploads/2010/01/sd-card-pinout.png for pin reference)
CD/DAT3/CS - GPIO5
CMD/DI - GPIO13
CLK/SCLK - GPIO14
DAT0/DO - GPIO12
(If you use an ESP12, take care: on all ESP12s I have, the silkscreen has GPIO4 and
GPIO5 switched around.)

One final note: because the root now is the root of the SD-card, the wifi connection
page isn't automatically loaded anymore. Go to 
http://[ip]/wifi/wifi.tpl to configure WiFi.