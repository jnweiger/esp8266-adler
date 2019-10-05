# Esp8266-Adler

Electronics and software used to "electrify" a steam model locomotive named "Adler".
Done with visual-studio code and platformio. 
Based on https://github.com/jnweiger/esp8266-esp8266-ota-blink-blink

This code enables OTA (over-the-air) programming of a wemos or esp8266 device.
It features the standard wlan-chooser procedure via an initial access-point (SSID: Adler_AutoAPxxxxxx).
It connects to the selected wlan with a DHCP ID Adler_xxxxxx.

HTTP port 80 has a simple form with buttons to navigate the model (both on floor and on rails) and trigger special effects.

The mechanical model of the "Adler" locomotive is available from https://www.thingiverse.com/thing:3603323

The main drive is a mini servo, with one stage of gears removed, and all electronics removed. Instead the motor is connected to a DRV88333 h-bridge, which we control through two PWM outputs. 
The library found by `pio lib install DRV8833` only suports mbed framework, not arduino.

With L293D driver, we would have enable pins.
https://roboindia.com/tutorials/nodemcu-motor-driver-pwm/ connects them to NodeMCU D5, D6 -- 
so we probably have PWM support on these pins.
The digital outputs are D0, D1, D2, D3

 * DRV8833 PWM settings are
 *          x_PWM1 x_PWM2    Mode
 *            0      0       Coast/Fast decay
 *            0      1       Reverse
 *            1      0       Forward
 *            1      1       Brake/slow decay
 * We should use pwm so that it toggles beteen a drive direction and free open
 * outputs. Let's assume that this is the 0 0 coasting mode. The fast and slow
 * decay labels are confusing. (I had assumed that Brake is the fast decay....)
 *
 * Arduino library defines software PWM: D1-D8 are pin 1-8, RSV is pin 12.
 * analogWrite(pin, dutycycle): pin 1..12; Enables software PWM on the specified pin. dutycycle is 0..1023
 * analogWrite(pin, 0): Disables PWM on specified pin.
 * analogWriteFreq(new_frequency): PWM frequency is 1kHz by default. PWM frequency is in the range 1 – 1000Khz.
 * 
 * The DataSheet defines hardware PWM:
 * Name__PinNum_IO______Function
 * MTDI     10  IO12    PWM0
 * MTDO     13  IO15    PWM1
 * MTMS     9   IO14    PWM2
 * GPIO04   16  IO4     PWM3
 *
 */


We use D1+D2 aka (GPIO04 + GPIO05) for the motor PWM.

