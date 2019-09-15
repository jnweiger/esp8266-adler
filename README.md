# Esp8266-Adler

Electronics and software used to "electrify" a steam model locomotive named "Adler".
Done with visual-studio code and platformio. 
Based on https://github.com/jnweiger/esp8266-esp8266-ota-blink-blink

This code enables OTA (over-the-air) programming of a wemos or esp8266 device.
It features the standard wlan-chooser procedure via an initial access-point (SSID: Adler_AutoAPxxxxxx).
It connects to the selected wlan with a DHCP ID Adler_xxxxxx.

HTTP port 80 has a simple form with buttons to navigate the model (both on floor and on rails) and trigger special effects.

The mechanical model of the "Adler" locomotive is available from https://www.thingiverse.com/thing:3603323
