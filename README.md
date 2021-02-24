# stevesch-PageHandler

Provides automatic reflection of variables between an ESP32 microcontroller web server and the web clients displaying the HTML pages of that server.

The library example uses stevesch-WiFiConnector for simple wifi setup (with config portal), but other wifi intialization may be used as long as an AsyncEspWebServer object is provided to this library's setup routine.

![Example Screencap](examples/minimal/example-minimal-screencap.jpg)

# Building and Running

To get up-and-running:
- PlatformIO is recommended
- Build this library (builds its own minimal example)
- Upload to any ESP32 board
- When your board is booted, a WiFi config portal named "PageHandler-Test" should be available.
- Connecting to the "PageHandler-Test" WiFi network with any device and set the config options for the WiFi network of your router (choose your network/SSID and set the password).
- Connect to your router's WiFi network and open the ESP32 page in any browser.  The IP of the device will be displayed in the serial output if you're monitoring serial output of the board.  If your router supports mDNS, you should be able to open "http://PageHandler-Test.local/"
(otherwise you'll need to open, e.g. "http://192.168.0.nnn" where nnn matches the address displayed in the serial output from your board).
