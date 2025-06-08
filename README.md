# dustmover
sensor sytem for detection of position and acceleration with data logging on SD-Card

*Usage of*
- SD-Card (for logging)
- ADXL 345 acceleration sensor 

# SD-Card Interface (ESP32 used) (VDD 3.3V)

CS (SS) = GPIO5
DI (MOSI) = GPIO23
VDD = 3.3 V
SCK (SCLK) = GPIO18
VSS = GND
DO (MISO) = GPIO19
 
For more info see file README.md in this library or on URL:
https://github.com/espressif/arduino-esp32/tree/master/libraries/SD

# ADXL345 (I2C interface used) (VDD 3.3V)
- Int1 (connected to GPIO27)
- Int2 (connected to GPIO2)
- SCL (GPIO22)
- SDA (GPIO21)

