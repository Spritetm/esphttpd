# esp-httpd README #

This is the demonstration project for the small but powerful libesphttpd webserver 
for ESP8266(EX) chips. It is an example of how to make a module that can have 
the AP it connects to configured over a webbrowser. It also illustrates multiple 
flash layouts and some OTA update functionality.

## ABOUT THE WEBSERVER ##

The Good (aka: what's awesome)
 - Supports multiple connections, for eg simultaneous html/css/js/images downloading
 - Static files stored in flash, in an (optionally compressed) RO filesystem
 - Pluggable using external cgi routines
 - Simple template engine for mixed c and html things

The Bad (aka: what can be improved)
 - Not built for speediness, although it's reasonable fast.
 - Built according to what I remember of the HTTP protocol, not according to the
   RFCs. Should work with most modern browsers, though.
 - No support for https.

The Ugly (aka: bugs, misbehaviour)
- Possible buffer overflows (usually not remotely exploitable) due to no os_snprintf
  This can be theoretically remedied by either Espressif including an os_snprintf in 
  their libs or by using some alternate printf lib, like elm-chans xprintf

## SOURCE OF THIS CODE ##
The official esphttpd repo lives at http://git.spritesserver.nl/esphttpd.git/ and
http://git.spritesserver.nl/libesphttpd.git/ . If you're a fan of Github, you can also
peruse the official mirror at https://github.com/Spritetm/esphttpd and https://github.com/Spritetm/libesphttpd . If
you want to discuss this code, there is a subforum at esp8266.com: http://www.esp8266.com/viewforum.php?f=34 .


## ABOUT THE EXAMPLE ##

When you flash the example into an ESP8266(EX) module, you get a small webserver with a few example
pages. If you've already connected your module to your WLAN before, it'll keep those settings. When
you haven't or the settings are wrong, keep GPIO0 for >5 seconds. The module will reboot into
its STA+AP mode. Connect a computer to the newly formed access point and browse to 
http://192.168.4.1/wifi in order to connect the module to your WiFi network. The example also
allows you to control a LED that's connected to GPIO2.

## BUILDING EVERYTHING ##

For this, you need an environment that can compile ESP8266 firmware. Environments for this still
are in flux at the moment, but I'm using esp-open-sdk: https://github.com/pfalcon/esp-open-sdk .
You probably also need an UNIX-like system; I'm working on Debian Linux myself. 

To manage the paths to all this, you can source a small shell fragment into your current session. For
example, I source a file with these contents:

    export PATH=${PWD}/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
    export SDK_BASE=${PWD}/esp-open-sdk/sdk
    export ESPTOOL=${PWD}/esptool/esptool.py
    export ESPPORT=/dev/ttyUSB0
    export ESPBAUD=460800

Actual setup of the SDK and toolchain is out of the scope of this document, so I hope this helps you
enough to set up your own if you haven't already. 

If you have that, you can clone out the source code:

    git clone http://git.spritesserver.nl/esphttpd.git/

This project makes use of heatshrink, which is a git submodule. To fetch the code:

    cd esphttpd
    git submodule init
    git submodule update

Now, build the code:

    make

## FLASHING ##
Flash the code happens in 2 steps. First the code itself gets flashed. Reset the module into bootloader
mode and enter `make flash`.

The 2nd step is to pack the static files the webserver will serve and flash that. Reset the module into
bootloader mode again and enter `make htmlflash`.

You should have a working webserver now.

## WRITING CODE FOR THE WEBSERVER ##

...errm... to be done. For now, look at the examples. Hey, you probably managed to find out how
the SDK works, this shouldn't be too hard :P


## CHANGE FROM SDK 0.9.3 (and earlier) TO SDK 0.9.4 (and later)
Change all occurences of `espconn_sent(connData->conn, (uint8 *)buff, len);` to `httpdSend(connData, buff, len)`
please. The reason for this is that you can't do multiple `espconn_sent` calls serially anymore, so
httpd needs to buffer the writes now. This is only needed in your own code; the code that comes
with httpd already has this changed.


