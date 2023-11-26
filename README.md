# offline-LightningATM-esp32
Bitcoin ATM (coins only) with lightning network support, running fully offline on an esp32. 
The lightning network is a 2nd layer on top of the bitcoin protocol enabling trustless, fast transactions with instant settlement and cheap fees.

## Special thanks to
@21isenough and contributors for the 3d models and inspiration:
https://github.com/21isenough/LightningATM

LNBits, Ben Arc and Stepan Snigirev for the cryptograpy used to make this ATM working without internet connection
https://github.com/lnbits/fossa
https://lnbits.com/

Axel Hambuch for the very detailed guide in german language on assembly of the electronics
https://ereignishorizont.xyz/lightning-atm/

## Used parts
All the parts are available on eBay and Aliexpress

* ESP32 NodeMCU Dev Board | Any "normal" esp32 dev board should do the job here
* DC-DC Adjustable Step-up Boost Power Supply LM2587S 5V -> 12V | for the coin acceptor, runs on 12V
* Waveshare 1.54 inch e-Paper Display Modul with SPI Interface
* Programmable Coin Acceptor (HX-616) - 6 Coin
* 10mm Metal Push Button Switch 3-6V with Yellow LED, Self-reset Momentary
* USB Type C socket | to plug in the power supply (i used a Raspberry Pi type C power supply)
* Little Mosfet modules ("15A 400W MOS FET Trigger") | To block the coin acceptor at certain points
* Orange PLA Filament for the 3D Printer
* Jumper Wires
* Heat-Set Threaded Inserts M3

All in all would calculate around $100 for the neccessary parts

## Assembly

1.)
Connect the Waveshare 1.54 inch display to the following pins on the ESP32:

| Display Pins                                                        | ESP32 GPIO |
|---------------------------------------------------------------------|------------|
| Busy                                                                | 27         |
| RST                                                                 | 33         |
| DC                                                                  | 25         |
| CS                                                                  | 26         |
| CLK                                                                 | SCK = 18   |
| DIN                                                                 | MOSI = 23  |
| GND                                                                 | GND        |
| 3.3V                                                                | 3.3V       |

2.)
Program the coin acceptor, be careful to adjust voltage of the step-up converter first before connecting the coin acceptor.
Find guide here:
English: https://github.com/21isenough/LightningATM/blob/master/docs/guide/coin_validator.md
German: https://ereignishorizont.xyz/lightning-atm/

3.) Connect the coin acceptor to the esp32
    Connect the 2 coin acceptor pins below the switch in short circuit with the mosfet on GND IN and GND OUT.
    Connect mosfet gnd pin to esp32 gnd and pwm pin to the pin specified in the include/lightning_atm.h file

4.) Connect LED Button to the Pins specified in the include/lightning_atm.h file

| Periphery Pin                                                        | ESP32 GPIO |
|----------------------------------------------------------------------|------------|
| Coin acceptor 'coin pin'                                             | 17         |
| Button LED Pin (+ after resistor)                                    | 13         |
| Button PIN 1                                                         | 32         |
| Button PIN 2                                                         | GND        |
| Mosfet PWM Pin                                                       | 12         |
| Mosfet GND Pin                                                       | GND        |

You can find inspiration on how to correctly wire everything in these docs by 21isenough
https://github.com/21isenough/LightningATM/tree/master/docs

## Setup software

Install VSCode and the PlattformIO IDE Extension.
Clone this repository and open it as PlattformIO Project.
Check the values in the file lightning_atm.h in the includes directory, change the LNBits key and URL, You can see the correct GPIO pins there.
Flash the software on the esp32. You may have to disconnect the ESP32 from the step up converter before connecting it to the computer to prevent faults, or power it up with the power supply and use an usb isolator.

If you need help ask me on Nostr @npub1z9n5ktfjrlpyywds9t7ljekr9cm9jjnzs27h702te5fy8p2c4dgs5zvycf
If this software and guide provided value to you feel free to send some sats to x@lnaddress.com
