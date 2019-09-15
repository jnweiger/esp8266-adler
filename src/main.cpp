/*
 * Accesspoint Basic OTA
 * 
 * It is a complex thing. OTA can be done in different ways.
 * - My prefered way would be: Open an accesspoint, run the loop, while accepting either OTA program uploads or 
 *   accepting blynk style commands. This is most robust, but not mainstram, and not even supported, it seems.
 * - Mainstream: if there is a network config from a previous run, re-use that. If it connects, run the code, accept commands.
 *   If that network does not connect, open an accesspoint, that allows entering a network config. then try to connect there,
 *   if it fails, restart. That requires a working WLAN.
 *   Then, OTA is done through that WLAN we connect to.
 * 
 * http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html
 * https://github.com/tzapu/WiFiManager/blob/master/examples/AutoConnect/AutoConnect.ino
 * 
 * Requires: WiFiManager. (PIO Home -> Open -> Libraries ... search is blocked. Hmm.)
 *                        Edit platformio.ini: Add lib_deps = WiFiManager@>=0.14
 * 
 * Caution: It still compiles if WiFiManager is misspelled in platformio.ini
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

// needed for WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// needed for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

WiFiServer webserver(80);            // outside of setup and loop, so that both can use it.

void setup() {
  // led is an output
  pinMode(LED_BUILTIN, OUTPUT);
  // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN, LOW);
  delay(100); 
  // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100); 
  // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN, LOW);
  // start serial
  Serial.begin(115200);           // debug to Serial is automtically enabled in autoConnect()

  // start wifi
  WiFiManager wifimanager;
  wifimanager.setDebugOutput(true);   // do or don't talk to Serial. (default true)
  wifimanager.setTimeout(120);        // 2 minutes wait.
  // autoConnect fetches ssid and pass from eeprom (if stored) and tries to connext.
  // if it does not connect, it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuraiton.

  if(!wifimanager.autoConnect("AdlerAutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  webserver.begin();

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("OTA Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

int nblinks = 2;
String header;

void loop() {
  // put your main code here, to run repeatedly ...

  Serial.printf("nblinks=%d\r\n", nblinks);  
  ArduinoOTA.handle();

  for (int i = 0; i < nblinks; i++)
    {
      // turn the LED on by making the voltage LOW
      digitalWrite(LED_BUILTIN, LOW);
      delay(100); 
      // turn the LED off (HIGH is the voltage level)
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }

  delay(900);

  WiFiClient client = webserver.available();
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println("Cache-Control: no-cache");    // wichtig! damit Daten nicht aus dem Browser cach kommen
            client.println();
            
            // adjust number of blinks
            int pos = header.indexOf("GET /set?nblinks=");
            if (pos >= 0) {
              char ch = header.charAt(pos+17);
              if (ch > '0' && ch <= '9') nblinks = ch - '0';
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP8266 OTA Blinker</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 5  
            client.printf("<p>nblinks=%d</p>\n", nblinks);
            client.println("<p><a href=\"/set?nblinks=1\"><button class=\"button\">1</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=2\"><button class=\"button\">2</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=3\"><button class=\"button\">3</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=4\"><button class=\"button\">4</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=5\"><button class=\"button\">5</button></a></p>");
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
