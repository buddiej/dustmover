# ICM-42688-Sensormodul – SPI-Pinbeschreibung zum Löten

Diese Verdrahtung ist für ein übliches ICM-42688/ICM-42688-P-Breakout mit
4-Draht-SPI und einem klassischen ESP32 DevKit vorgesehen.

## Zuordnung

| Pin am Sensormodul | ESP32 DevKit | Kabelfarbe | Funktion | Löt-Hinweis |
| --- | --- | --- | --- | --- |
| `VCC`, `3V3` oder `VDD` | `3V3` | Rot | Versorgung | Nur 3,3 V verwenden, sofern das Modul nicht ausdrücklich einen 5-V-Eingang besitzt. |
| `GND` | `GND` | Weiß | Masse | Gemeinsame Masse ist zwingend. |
| `SCLK`, `SCL` oder `AP_SCLK` | `GPIO18` | Gelb | SPI-Takt | Nicht mit I²C-SCL verwechseln; im SPI-Betrieb ist dies der Takt. |
| `SDI`, `SDA`, `SDIO` oder `MOSI` | `GPIO23` | Grün | Daten zum Sensor | Im 4-Draht-SPI: MOSI → SDI. |
| `SA0`, `SDO`, `MISO` oder `AP_SDO` | `GPIO19` | Braun | Daten vom Sensor | Der GY-601N1 nennt diesen Pin SA0. |
| `CS`, `nCS` oder `AP_CS` | `GPIO5` | Weiß | Chip-Select, aktiv Low | GPIO5 ist ein Strapping-Pin. CS beim Start High halten. |
| `INT2` | `GPIO27` | Schwarz | Datenbereit-Interrupt | INT2 des Sensors direkt mit GPIO27 verbinden. |
| `INT1` | nicht anschließen | — | Optionaler Interrupt | Für diese Verdrahtung nicht benötigt. |

## Lötreihenfolge

1. ESP32 und Sensormodul spannungslos machen.
2. `GND` zuerst verbinden.
3. `3V3` verbinden.
4. Die vier SPI-Leitungen verbinden: `SCLK`, `SDI/MOSI`, `SDO/MISO`, `CS`.
5. `INT2` nach `GPIO27` führen. Im Schaltplan ist die Leitung schwarz gestrichelt.
6. Vor dem Einschalten mit dem Multimeter prüfen: kein Kurzschluss zwischen `3V3` und `GND`.

## Wichtige Hinweise

- Die Bezeichnungen und die physische Reihenfolge unterscheiden sich je nach
  Breakout. Nach dem **Signalnamen** löten, nicht nach der Pinposition in der
  Zeichnung.
- Viele Breakouts enthalten bereits Spannungsregler, Pegelanpassung,
  Pull-ups und Abblockkondensatoren. Ein zusätzliches `R1 = 10 kΩ` von `CS`
  nach `3V3` und `C1 = 100 nF` zwischen `3V3` und `GND` ist nur nötig, wenn
  diese Bauteile auf dem Modul fehlen.
- Für 4-Draht-SPI muss `CS` beim Zugriff Low sein. Für I²C wäre `CS` dagegen
  typischerweise High; diese Skizze ist ausschließlich für SPI.
- Der ICM-42688-P unterstützt SPI bis 24 MHz. Zum ersten Test empfiehlt sich
  eine niedrigere SPI-Frequenz, z. B. 1–8 MHz.

## Quelle der Signalnamen

Die Signalnamen entsprechen dem TDK/InvenSense-Datenblatt des ICM-42688-P:
`AP_CS`, `AP_SCL/AP_SCLK`, `AP_SDA/AP_SDIO/AP_SDI` und
`AP_SDO/AP_AD0`. Die konkrete Breakout-Platine kann diese Namen verkürzt als
`CS`, `SCLK`, `SDA/SDI` und `SDO` ausführen.
