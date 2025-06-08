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

Examples:
    listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello ");
    appendFile(SD, "/hello.txt", "World!\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");
    testFileIO(SD, "/test.txt");
 
For more info see file README.md in this library or on URL:
https://github.com/espressif/arduino-esp32/tree/master/libraries/SD

# ADXL345 (I2C interface used) (VDD 3.3V)
- Int1 (connected to GPIO27)
- Int2 (connected to GPIO2)
- SCL (GPIO22)
- SDA (GPIO21)

Examples:
/* Choose the data rate         Hz
    ADXL343_DATA_RATE_3200    3200
    ADXL343_DATA_RATE_1600    1600
    ADXL343_DATA_RATE_800      800
    ADXL343_DATA_RATE_400      400
    ADXL343_DATA_RATE_200      200
    ADXL343_DATA_RATE_100      100
    ADXL343_DATA_RATE_50        50
    ADXL343_DATA_RATE_25        25
    ADXL343_DATA_RATE_12_5      12.5  
    ADXL343_DATA_RATE_6_25       6.25
    ADXL343_DATA_RATE_3_13       3.13
    ADXL343_DATA_RATE_1_56       1.56
    ADXL343_DATA_RATE_0_78       0.78
    ADXL343_DATA_RATE_0_39       0.39
    ADXL343_DATA_RATE_0_20       0.20
    ADXL343_DATA_RATE_0_10       0.10
*/

/* Choose the measurement range
    ADXL343_RANGE_16G    16g     
    ADXL343_RANGE_8G      8g     
    ADXL343_RANGE_4G      4g   
    ADXL343_RANGE_2G      2g
*/ 



