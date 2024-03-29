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
 *           DRV8833
 * Caution: It still compiles if Library names are misspelled in platformio.ini
 *
 * DRV8833 PWM settings are
 *          x_PWM1 x_PWM2    Mode
 *            0      0       Coast/Fast decay
 *            0      1       Reverse
 *            1      0       Forward
 *            1      1       Brake/slow decay
 * We should use pwm so that it toggles beteen a drive direction and free open outputs. Let's assume that this is the
 * 0 0 coasting mode. The fast and slow decay labels are confusing. Fast decay means the current decays fast.
 * Drivers tristate, only the clamping diodes pull. Slow decay is when the wires are short circuited. Not sure what is better.
 * I had assumed that short circuit would be a strong current. But they call it fast decay, okay.
 *
 * We use GPIO04 and GPIO05 aka (D1 + D2) for motor PWM.
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
// #include <DRV8833.h>           // ouch, the official DRV8833 driver does not support arduino framework or wemos platforms

// needed for WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// needed for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// D4 == GPIO02 == BUILTIN_LED
// D1 == GPIO04
// D2 == GPIO05
#define MOTORPIN1 D2
#define MOTORPIN2 D1

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
  pinMode(MOTORPIN1, OUTPUT);	  // software PWM for main motor
  pinMode(MOTORPIN2, OUTPUT);	  // software PWM for main motor
  analogWriteFreq(50);            /* Set PWM frequency. 50Hz to match a standard servo */

  // start wifi
  WiFiManager wifimanager;
  wifimanager.setDebugOutput(true);   // do or don't talk to Serial. (default true)

#if 0
  // use setHostname() as soon as it is in master of github.com/tzapu/WiFiManager
  wifimanager.setHostname( ("Adler_" + String(ESP.getChipId(), HEX)).c_str() );
#else
  WiFi.hostname( ("Adler_" + String(ESP.getChipId(), HEX)).c_str() );               // ESP8266 only.
#endif
  wifimanager.setTimeout(120);        // 2 minutes wait.
  // autoConnect fetches ssid and pass from eeprom (if stored) and tries to connext.
  // if it does not connect, it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuraiton.

  if(!wifimanager.autoConnect( ("Adler_AutoAP" + String(ESP.getChipId(), HEX)).c_str() )) {
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

#define LOOP_DELAY 100		// 100: run 10 loops per second.
int motor_set_speed = 0;	// -1023 .. 0 1023; 160 minimum to break loose
int nblinks = 2;
String header;

int blinkstate = 0;
#define BLINK_PAUSE 3
int blinkcount = 0;

#define MOTOR_ACCEL 12		// from stop to full speed takes 1024/MOTOR_ACCEL*LOOP_DELAY*0.001 seconds.
int motor_cur_speed = 0;	// -1023 .. 0 1023

void loop() {

  Serial.printf("nblinks=%d\r\n", nblinks);
  ArduinoOTA.handle();

  if (!blinkstate)
    {
      // turn the LED on by making the voltage LOW
      if (blinkcount >= 0)
        digitalWrite(LED_BUILTIN, LOW);
      blinkstate = !blinkstate;
    }
  else
    {
      // turn the LED off (HIGH is the voltage level)
      if (blinkcount >= 0)
        digitalWrite(LED_BUILTIN, HIGH);
      blinkstate = !blinkstate;
      blinkcount++;
    }

  if (blinkcount >= nblinks)
    {
      blinkcount = -BLINK_PAUSE;
    }

  int motor_diff = motor_set_speed - motor_cur_speed;
  if (motor_diff > MOTOR_ACCEL)
    motor_diff = MOTOR_ACCEL;
  else if (motor_diff < -MOTOR_ACCEL)
    motor_diff = -MOTOR_ACCEL;
  motor_cur_speed += motor_diff;

  if (motor_cur_speed >= 0)
    {
      analogWrite(MOTORPIN1, motor_cur_speed);
      digitalWrite(MOTORPIN2, LOW);
    }
  else
    {
      digitalWrite(MOTORPIN1, LOW);
      analogWrite(MOTORPIN2, -motor_cur_speed);
    }

  delay(LOOP_DELAY);

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
            else if (header.indexOf("GET /stop") >= 0)
              {
                motor_set_speed = 0;
              }
            else if (header.indexOf("GET /fwd") >= 0)
              {
                // if (motor_set_speed < 0) motor_set_speed = 0;
                motor_set_speed += 128;
                if (motor_set_speed >= 1024) motor_set_speed = 1023;
              }
            else if (header.indexOf("GET /bwd") >= 0)
              {
                // if (motor_set_speed > 0) motor_set_speed = 0;
                motor_set_speed -= 128;
                if (motor_set_speed <= -1024) motor_set_speed = -1023;
              }
            else if (header.indexOf("GET /ffwd") >= 0)
              {
                motor_set_speed = 1023;
              }
            else if (header.indexOf("GET /fbwd") >= 0)
              {
                motor_set_speed = -1023;
              }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 12px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h4>Adler drive</h4>");

            // Display current state, and ON/OFF buttons for GPIO 5
            client.println("<table border=\"0\" cellpadding=\"10\" width=\"80%\"><tr><td width=\"40%\">");
            client.printf("<p>nblinks=%d</p>\n", nblinks);
            client.println("<p><a href=\"/set?nblinks=1\"><button class=\"button\">1</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=2\"><button class=\"button\">2</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=3\"><button class=\"button\">3</button></a></p>");
            client.println("<p><a href=\"/set?nblinks=4\"><button class=\"button\">4</button></a></p>");
            client.println("</td><td width=\"40%\">");
            client.printf("<p>speed=%d</p>\n", motor_set_speed);
            client.println("<p><a href=\"/ffwd\"><button class=\"button\">&gt;&gt;</button></a></p>");
            client.println("<p><a href=\"/fwd\"><button class=\"button\">&gt;</button></a></p>");
            client.println("<p><a href=\"/stop\"><button class=\"button\">[]</button></a></p>");
            client.println("<p><a href=\"/bwd\"><button class=\"button\">&lt;</button></a></p>");
            client.println("<p><a href=\"/fbwd\"><button class=\"button\">&lt;&lt;</button></a></p>");
            client.println("</td></tr></table>\n</body></html>");

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
