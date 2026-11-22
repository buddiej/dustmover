# DustMover – Fritzing-Verdrahtung

Diese Verdrahtung dokumentiert den früheren Aufbau mit ADXL345. Die aktuelle
Firmware verwendet den ICM-42688-P. [Aktuelle Verdrahtung](icm42688-spi-schematic.svg)

## Anschlüsse

| Quelle | Signal | Ziel | Hinweis |
| --- | --- | --- | --- |
| ESP32 DevKit | 3V3 | ADXL345 VCC | Nur 3,3-V-Logik |
| ESP32 DevKit | GND | ADXL345 GND | Gemeinsame Masse |
| ESP32 GPIO21 | SDA | ADXL345 SDA | I²C |
| ESP32 GPIO22 | SCL | ADXL345 SCL | I²C |
| ESP32 GPIO27 | DATA_READY / INT2 | ADXL345 INT2 | Synchronisiert die 100-Hz-Abtastung |
| ESP32 3V3 | High | ADXL345 CS | Für I²C-Modus, falls am Modul nicht fest vorgegeben |
| ESP32 GND | Low | ADXL345 SDO/ALT | Adresse 0x53 |
| ESP32 | 3V3 | SD VCC | Breakout-Versorgungsvorgabe beachten |
| ESP32 | GND | SD GND | Gemeinsame Masse |
| ESP32 GPIO5 | CS/SS | SD CS | GPIO5 ist ein Strapping-Pin |
| ESP32 GPIO23 | MOSI/DI | SD MOSI/DI | SPI |
| ESP32 GPIO19 | MISO/DO | SD MISO/DO | SPI |
| ESP32 GPIO18 | SCK | SD SCK | SPI |

## Alternative: ADXL345 über einen separaten SPI-Bus

Die SVG enthält zusätzlich eine getrennte SPI-Variante für den ADXL345. Der
ADXL345 teilt sich dabei keine SPI-Leitung mit der SD-Karte:

| ADXL345-Anschluss | ESP32 | Hinweis |
| --- | --- | --- |
| VCC | 3V3 | 3,3-V-Versorgung |
| GND | GND | Gemeinsame Masse |
| CS | GPIO26 | Separater Chip-Select; GPIO27 bleibt für I²C DATA_READY reserviert |
| SCLK | GPIO25 | Separater SPI-Takt |
| SDI / MOSI | GPIO33 | Sensor-Eingang |
| SDO / MISO | GPIO32 | Sensor-Ausgang |

Die SD-Karte bleibt auf GPIO5/18/19/23. Für diese Variante muss der ADXL345
im 4-Wire-SPI-Modus betrieben werden: `CS` low aktiviert SPI, `SDA/SDI/SDIO`
ist MOSI und `SDO/ALT ADDRESS` ist MISO. Das I²C-Pull-up-/Adressschema gilt
dann nicht als Buskonfiguration.

Die frühere Live-Firmware verwendete I²C. Für die SPI-Option
muss der Sensor im Code beispielsweise mit `ADXL345_WE(SPIClass*, cs, true,
mosi, miso, sck)` und einem separat initialisierten `SPIClass` aufgebaut
werden.

Alle Signalleitungen müssen 3,3-V-kompatibel sein. Eine nackte microSD-Karte
darf nicht direkt mit 5 V versorgt werden. Bei Breakout-Modulen sind
Versorgung, Pegelwandler und vorhandene Pull-ups anhand des konkreten Moduls
zu prüfen.

Die frühere Live-Debug-Firmware nutzte den ADXL345 über I²C und streamte
Rohwerte über UART. In diesem Live-Modus blieb die SD-Karte ungenutzt.

Wichtig: GPIO2 bleibt offen, weil es ein ESP32-Strapping-Pin ist. Für diese
Firmware wird `INT2 → GPIO27` benötigt; die alte `INT1 → GPIO27`-Verbindung
entfernen.

## Fritzing-Aufbau

1. Generic ESP32 DevKit bzw. ESP32-WROOM-32 einsetzen.
2. Ein ADXL345-I²C-Breakout und ein microSD-SPI-Breakout einsetzen.
3. Leitungen gemäß Tabelle und SVG verbinden.
4. Bei abweichenden Breakout-Pinbeschriftungen die Signalnamen, nicht die
   physische Reihenfolge, maßgeblich nehmen.

