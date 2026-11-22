# DustMover: Toolchain und Projektstart

## Empfehlung

Für den ESP32 mit ICM-42688-P eignet sich **VS Code + PlatformIO +
Arduino-ESP32 mit FreeRTOS**. Die Standardfirmware streamt Messdaten über UART.

Diese Basis implementiert einen Sensorlogger. Die mechanische Rohrbeförderung,
Antriebe und deren Steuerung sind noch nicht spezifiziert.

| Möglichkeit | Eignung für dieses Projekt |
| --- | --- |
| Arduino + FreeRTOS über PlatformIO | Empfohlener Einstieg: vorhandener Arduino-Code, Tasks, Queues und ESP32-Timer direkt nutzbar. |
| ESP-IDF mit FreeRTOS | Gute spätere Wahl für tiefe Treiberkontrolle, Diagnose und hohe Anforderungen an das Timing; mehr Portierungsaufwand. |
| Zephyr | Interessant bei geplanter Unterstützung verschiedener MCU-Familien; für den bestehenden Arduino-Sketch zusätzlicher Integrationsaufwand. |
| Arduino IDE 2 | Einfacher alternativer Editor; dieselbe Firmware und dieselben FreeRTOS-Funktionen bleiben nutzbar. |

Windows, macOS und Linux sind die unterstützten Entwicklungsrechner-Systeme.
Das Laufzeitsystem auf dem Controller ist FreeRTOS. Die gepinnte Konfiguration
ist für einen klassischen **ESP32 DevKit / ESP32-WROOM-32** gedacht. ESP32-S3,
C3 und andere Boards brauchen passende Boarddefinitionen und Pinbelegungen.

## Was bereitliegt

```text
platformio.ini                      Board und Framework
firmware/DustMover/Config.h          SPI-Pins, Interrupt-Option und Puffergröße
firmware/DustMover/Icm42688.*         SPI-Treiber für den ICM-42688-P
firmware/DustMover/DustMover.ino     Messaufgabe, UART und SD-Aufgabe
tools/flash_and_test.py              Flashen und Selbsttest über die serielle Ausgabe
dustmover.ino                        ursprünglicher Sketch als Referenz
```

PlatformIO verwendet `espressif32@6.10.0` mit Arduino-ESP32 2.0.17.
`esp32dev` ist der Standardbuild mit ESP-IDF-SPI3 und UART-Ausgabe.

## Einrichtung und erster Build

1. VS Code installieren, darin die Erweiterung **PlatformIO IDE** installieren.
2. Den Projektordner mit `platformio.ini` öffnen.
3. **PlatformIO: Build** ausführen. Beim ersten Mal werden Toolchain und Bibliotheken geladen.
4. ESP32 per USB-Datenkabel verbinden, **Upload** ausführen.
5. Den seriellen Monitor mit **230400 Baud** öffnen.

Alternativ mit installiertem PlatformIO Core aus dem Projektordner:

```sh
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 230400
```

Bei mehreren seriellen Geräten den passenden Port über `--upload-port` auswählen;
für den Monitor `--port` verwenden. Unter Linux können serielle Gruppenrechte,
unter Windows/macOS ein zum USB-Chip passender Treiber nötig sein.

### USB-Seriell-Treiber

Der ESP32 DevKit meldet sich über einen USB-Seriell-Wandler am Rechner an.
Welcher Chip verbaut ist, steht auf dem kleinen IC neben der USB-Buchse.

| Wandler | Windows | macOS | Linux |
| --- | --- | --- | --- |
| Silicon Labs CP2102/CP2104 | [CP210x VCP-Treiber](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) installieren | ab macOS 10.15 meist eingebaut, sonst CP210x VCP-Treiber | im Kernel enthalten (`cp210x`) |
| WCH CH340/CH341/CH9102 | [CH341SER-Treiber](https://www.wch-ic.com/downloads/CH341SER_EXE.html) installieren | neuere Versionen eingebaut, sonst [CH34x-Treiber](https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html) | im Kernel enthalten (`ch341`) |

- Erscheint kein Port (Windows: Geräte-Manager → *Anschlüsse (COM & LPT)*;
  macOS/Linux: `pio device list`), zuerst ein anderes USB-Kabel versuchen:
  reine Ladekabel haben keine Datenleitungen.
- Linux: Den Benutzer mit `sudo usermod -aG dialout $USER` in die Gruppe
  `dialout` aufnehmen und neu anmelden. Bei CH340 kann das Paket `brltty`
  den Port belegen; dann `sudo apt remove brltty`.
- Verbindet sich der Upload nicht (`Connecting......`), die Taste **BOOT**
  gedrückt halten, bis der Upload startet.

### Flashen und Selbsttest

`tools/flash_and_test.py` baut die Firmware, lädt sie hoch, startet den ESP32
neu, zeichnet die serielle Ausgabe 10 s lang auf und prüft sie. Voraussetzungen:
PlatformIO Core und `pip install pyserial`. Den Sensor dabei ruhig liegen lassen.

```sh
python tools/flash_and_test.py                # Port automatisch suchen
python tools/flash_and_test.py --port COM5    # Port vorgeben
python tools/flash_and_test.py --no-upload    # nur die vorhandene Firmware testen
python tools/flash_and_test.py --env esp32dev_sd
```

Geprüft werden: keine `ERROR`-Zeilen, `WHO_AM_I=0x47`, Start der Ausgabe,
Abtastrate innerhalb von 10 % um 100 Hz, monoton steigende Zeitstempel,
`read_errors`, `coalesced` und `queue_drops` gleich null, Betrag der
Beschleunigung von etwa 1 g und sich ändernde Messwerte. Das Skript endet
mit `RESULT: PASS` oder `RESULT: FAIL`. Die vollständige Ausgabe steht in
`.pio/flash-test.log`.

Für Arduino IDE 2 das Boardverwalter-Paket `esp32 by Espressif Systems` in
Version 2.0.17 installieren. `firmware/DustMover/DustMover.ino` öffnen und
**ESP32 Dev Module** und den Port wählen. `Config.h`, `Icm42688.h` und
`Icm42688.cpp` gehören in denselben Sketchordner.

## Verdrahtung

| ESP32 | GY-601N1 | Funktion |
| --- | --- | --- |
| GPIO18 | SCL | SCK |
| GPIO23 | SDA | MOSI |
| GPIO19 | SA0 | MISO |
| GPIO5 | CS | Chip-Select |
| GPIO27 | INT2 | DATA_READY |
| GND | GND | Masse |
| 3V3 | VCC | 3,3-V-Versorgung |

Für diesen Aufbau den Kartenleser vom SPI-Bus trennen. Die Versorgung und
die Signalleitungen des Sensormoduls verwenden 3,3 V.
GPIO5 ist ein Strapping-Pin. Der Sensor-CS soll beim Start inaktiv (High) sein.

Die Firmware setzt CS, SCK und MOSI als Ausgänge. Vor der SPI-Initialisierung
setzt sie CS auf High und SCK und MOSI auf Low. SPI-Modus 0 hält SCK in
Ruhe auf Low. MISO ist ein Eingang ohne internen Pull-Widerstand. INT2 ist
ein Eingang mit Pull-down und löst bei steigender Flanke aus.

Bei einer unerwarteten Sensor-ID wiederholt der Standardbuild die Abfrage
mit schwachem Pull-up und anschließend mit Pull-down an MISO. Wenn die
Bytes dem eingestellten Pull folgen, hat der Sensor die Leitung bei dieser
Abfrage wahrscheinlich nicht getrieben. Danach ist MISO wieder ohne Pull.

Die Firmware wartet auf steigende Data-Ready-Flanken an GPIO27. Dazu
`INT2` am Sensor anschließen. Die Ausgabe von Messwerten beginnt mit der
ersten Flanke. Für einen Aufbau ohne INT2 kann
`useDataReadyInterrupt` in `Config.h` auf `false` gesetzt werden.

## Messablauf

```text
ICM-42688-P mit nominell 100 Hz
    → Data-Ready-Interrupt von INT2 an GPIO27
    → Messaufgabe liest sechs Rohwerte per SPI
    → UART mit 230400 Baud
```

Die mit `RAW,` markierten UART-Zeilen verwenden folgende CSV-Felder.

```text
t_us,sequence,data_ready_events,coalesced,queue_drops,read_errors,x_raw,y_raw,z_raw,gx_raw,gy_raw,gz_raw
```

`t_us` erfasst den ESP32-Zeitstempel in Mikrosekunden nach dem Lesen. Die
Beschleunigung hat einen Messbereich von ±2 g und eine Skalierung von
16 384 Schritten pro g. Die Drehrate hat ±250 °/s und etwa 131 Schritte pro °/s.
`sequence` zählt im Abfragemodus gelesene Messungen und im Interruptmodus
beobachtete Flanken. `coalesced` zählt im Interruptmodus zusammengefasste
Ereignisse. `read_errors` ist kumulativ. `queue_drops` bleibt im
Standardbuild für die Kompatibilität mit dem Logger-Format enthalten und
hat den Wert null.

## Build mit microSD

Mit `pio run -e esp32dev_sd` lässt sich der Logger bauen. Diese Variante
nutzt den Arduino-SPI-Treiber gemeinsam mit der SD-Bibliothek. Der Leser teilt
SCK, MOSI/DI und MISO/DO mit dem Sensor und verwendet GPIO26 als eigenen CS.
Der Leser muss MISO freigeben, wenn sein CS inaktiv ist. Die Versorgung
richtet sich nach dem Lesermodul.

Mit einer verfügbaren FAT32-Karte erzeugt dieser Build pro Neustart die nächste
Datei `dm00000.csv`, `dm00001.csv` usw. und synchronisiert Daten ungefähr
einmal pro Sekunde. Bei SD-Fehlern läuft die UART-Ausgabe weiter. Längere
SD-Zugriffe können die Sensorabfrage am gemeinsamen SPI-Bus verzögern.

## Befund zum Original

Geprüft: [dustmover.ino, Commit af1524e](https://github.com/buddiej/dustmover/blob/af1524e488445834498c97587a4f9d3c9db32b91/dustmover.ino).

- Der DATA_READY-Interrupt wird auf `INT_PIN_2` konfiguriert, gewartet wird über
  `int2Pin` auf GPIO2. Damit passt das erwartete Ereignis nicht zur dokumentierten Verdrahtung.
- `while(!int2event){}` wartet ohne Timeout und ohne Freigabe an den Scheduler.
- Während Lesen und Schreiben wird `setMeasureMode(false)` gesetzt: Messpausen entstehen.
- Die SD-Datei wird pro Datensatz mehrfach geöffnet bzw. geschlossen.
- `memset(..., '/0', ...)` benutzt kein Nullbyte; gemeint wäre `'\0'`.
- `strcat` baut ohne Längenprüfung eine lange Zeile in einem 128-Byte-Puffer auf.
  Die möglichen Feldlängen überschreiten dessen Kapazität.
- Nach fehlgeschlagener Initialisierung gibt `setup()` zurück, aber `loop()`
  versucht dennoch die Erfassung. Die neue Basis startet Tasks erst nach erfolgreicher Initialisierung.

Der ADXL345 misst Beschleunigung; statische Neigung lässt sich unter geeigneten
Bedingungen ableiten. Absolute Rohrposition folgt daraus nicht zuverlässig.
Luftverwirbelung lässt sich damit allenfalls indirekt über mechanische Vibration
untersuchen; direkte Strömungsmessung benötigt einen passenden Sensor.

## Nächster technischer Ausbau

Zuerst die benötigte Signalbandbreite und Messdauer festlegen. Die 100 Hz hier
sind ein Inbetriebnahmewert, keine begründete Rate für das spätere Messziel.
Für verlustfreie Vibrationsmessung: FIFO im Stream-Modus mit WATERMARK/OVERRUN-
Überwachung, konsistente Sampleblöcke und dokumentierte Zeitbasis verwenden.
Für längere Messungen bleiben Dateirotation, kontrollierter Stopp und
Stromausfallverhalten zu prüfen.

## Prüfung auf der Hardware

1. Mit dem Sensormodul starten. Der Monitor meldet `WHO_AM_I=0x47`
   und `INT2 data ready on GPIO27`. Bei
   ankommenden Interrupts steigen `seq` und `sampled` im Monitor.
2. Ohne Sensor erscheint eine Fehlermeldung.
3. Sensor ruhig hinlegen: ein plausibler Schwerkraftvektor von ungefähr 1 g,
   Achsenwerte ändern sich beim Kippen.
4. Eine Minute aufzeichnen: nominell etwa 6000 Zeilen, monoton steigende Zeitstempel,
   Abstände ungefähr 10000 µs; Verlustzähler prüfen.

Diese Prüfschritte sind ein Inbetriebnahmeplan; ohne angeschlossene Hardware
lassen sich elektrische Funktion und tatsächliches Timing hier nicht bestätigen.

## Dokumentation der verwendeten Werkzeuge

- [PlatformIO ESP32-Plattform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [ESP-Timer](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/esp_timer.html)
- [FreeRTOS in ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/freertos.html)
- [TDK InvenSense ICM-42688-P, DS-000347 (PDF)](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000347-icm-42688-p-v1.6.pdf)
- [Analog Devices ADXL345 datasheet, Rev. G (PDF)](https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf)
- [Analog Devices ADXL345 product page](https://www.analog.com/en/products/adxl345.html)
