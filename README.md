# loraPython

Python wrapper for a LoRa Raspberry Pi HAT.

It was tested with RFM95W LoRa module. Therefore it should work with following modules:

- RFMW95W
- SX1278
- SX1276

Additional information on how to use the driver can be found on 
[my blog](https://blog.domski.pl/lora-driver-for-raspberry-pi-with-python-wrapper).

# API Documentation

The Python wrapper for a LoRa module offers three functions
- init()
- recv()
- send()

## init(mode, frequency, spread_factor)

Initializes LoRa module. Has to be invoked every time 
when the mode (transmitter/receiver) is being changed.

Takes 3 arguments:
- **mode** (0 for transmitter, 1 for receiver),
- **frequency** expressed in Hertz (make sure the LoRa module is compatible with selected frequency),
- **spread_factor** Lora spread factor, available are 7, 8, 9, 10, 11 or 12.

Currently only bandwidth 125kHz and coding rate 4/5 are supported.

Returns 0 on success. 

## recv()

Returns a tuplet with 6 elements:

- buffer (bytes), 
- number of received bytes, 
- PRRSI last packet RSSI (received signal strength indicator) [dBm]
- RRSI current RSSI [dBm]
- SNR signal noise ratio
- error code (0, no error, != 0, otherwise)

If no data was received the second element of the tuplet is set to 0.

## send(data)

Takes 1 parameter:
- **data** byte array to be send.

# Connection

The module has to be connected to the Raspberry Pi according to the 
table below

|RFM95W pin | Raspberry Pi pin    | Description                   |
|-|-|-|
|3.3V	   | 3V3, pin 1             | 3V3 power supply              |
|GND     | GND, pin 6             | power ground                  |
|MISO    | GPIO 9 (MISO), pin 21  | SPI Master Input Slave Output |
|MOSI    | GPIO 10 (MOSI), pin 19 | SPI Master Output Slave Input |
|SCK     | GPIO 11 (SCLK), pin 23 | SPI clock                     |
|NSS     | GPIO 25, pin 22        | SPI chip select               |
|RESET   | GPIO 17, pin 11        | LoRa module reset             |
|DIO0    | GPIO 4 (GPCLK0), pin 7 | LoRa status line              |

# Compilation

Before compiling make sure the proper version in Makefile was selected.
This can be configured through *PYTHONVER* variable.

Additionally *wiringpi* library is used. Install it before 
compilation with

```bash
sudo apt install wiringpi
```

For the library and example application

```bash
make all
```

For library only

```bash
make lib
```

# Examples 

Before running the examples make sure that SPI interface is enabled in Raspberry Pi.
For this use command

```bash
sudo raspi-config
```

Navigate through the menu, enable SPI and restart RPi.

## Receiver:

Configuration fo a simple receiver at 868MHz freqency.

```Python
import loralib
loralib.init(1, 868000000, 7)
data=loralib.recv();
data
>>> (b'hello', 5, -25, -94, 9, 0)
```

### Receiver in a loop

Configuration of a receiver in a loop.

```Python
import loralib
import time

loralib.init(1, 868000000, 7)

for i in range(0,10000000):
  msg=loralib.recv()
  print("%06d, frame=" % i, end='')
  print(msg)
  time.sleep(1)    
```

Configuration of a receiver in a loop with high frequency check 
and verification of message size and CRC.

```Python
import loralib
import time

loralib.init(1, 868000000, 7)

for i in range(0,10000000):
  msg=loralib.recv()
  if msg[5] == 0 and msg[1] > 0:
    print(msg)
  time.sleep(0.001)    
```
  
## Transmitter:

Transmitter sending a string "hello".

```Python
import loralib                                                          
loralib.init(0, 868000000, 7)                                           
loralib.send(b'hello');
```




