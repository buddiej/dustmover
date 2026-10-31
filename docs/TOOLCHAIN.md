# DustMover: Toolchain und Projektstart

## Empfehlung

Für den vorhandenen ESP32 mit ADXL345 und SD-Karte: **VS Code + PlatformIO +
Arduino-ESP32 + das darin enthaltene FreeRTOS**. Damit lassen sich Arduino-Bibliotheken
weiterverwenden, während Messung und SD-Schreiben getrennte Aufgaben bekommen.
Ein zusätzliches Betriebssystem muss dafür nicht installiert werden.

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
platformio.ini                      Board, Framework und Bibliotheksversion
firmware/DustMover/Config.h          Pins, Abtastintervall und Puffergröße
firmware/DustMover/DustMover.ino     Timer, Messaufgabe und SD-Aufgabe
dustmover.ino                        ursprünglicher Sketch als Referenz
```

PlatformIO ist auf `espressif32@6.10.0` festgelegt (Arduino-ESP32 2.0.17),
die Sensorbibliothek auf `ADXL345_WE@3.1.0`. Das ist eine festgelegte Startbasis,
keine Behauptung über die jeweils neuesten Versionen. Versionswechsel separat
bauen und auf der Hardware prüfen.

## Einrichtung und erster Build

1. VS Code installieren, darin die Erweiterung **PlatformIO IDE** installieren.
2. Den Projektordner mit `platformio.ini` öffnen.
3. **PlatformIO: Build** ausführen. Beim ersten Mal werden Toolchain und Bibliotheken geladen.
4. ESP32 per USB-Datenkabel verbinden, **Upload** ausführen.
5. Den seriellen Monitor mit **115200 Baud** öffnen.

Alternativ mit installiertem PlatformIO Core aus dem Projektordner:

```sh
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Bei mehreren seriellen Geräten den passenden Port über `--upload-port` auswählen;
für den Monitor `--port` verwenden. Unter Linux können serielle Gruppenrechte,
unter Windows/macOS ein zum USB-Chip passender Treiber nötig sein.

**Arduino IDE 2:** Boardverwalter-Paket `esp32 by Espressif Systems` in Version
2.0.17 und Bibliothek `ADXL345_WE` in Version 3.1.0 installieren.
`firmware/DustMover/DustMover.ino` öffnen, Board **ESP32 Dev Module** und Port wählen,
kompilieren und hochladen. `Config.h` muss im selben Sketchordner bleiben.
Die FreeRTOS- und ESP-Timer-Header gehören bereits zum ESP32-Boardpaket.

## Verdrahtung

Die Belegung übernimmt die klassischen ESP32-Pins aus dem Repository:

| Bauteil | Anschluss | ESP32 |
| --- | --- | --- |
| ADXL345 | SDA | GPIO21 |
| ADXL345 | SCL | GPIO22 |
| ADXL345 | Versorgung / GND | 3,3 V / GND |
| ADXL345 | CS | High / 3,3 V für I²C, falls das Modul dies nicht bereits vorgibt |
| ADXL345 | SDO/ALT ADDRESS | GND für Adresse 0x53; High ergibt 0x1D |
| SD | CS | GPIO5 |
| SD | MOSI / DI | GPIO23 |
| SD | MISO / DO | GPIO19 |
| SD | SCK | GPIO18 |
| SD | Versorgung / GND | gemäß Modulbeschaltung / gemeinsame Masse |

Alle Signalleitungen müssen mit **3,3-V-Logik** kompatibel sein. Eine nackte
microSD-Karte wird mit 3,3 V versorgt; bei Breakouts deren Schaltung und
Versorgungsvorgabe prüfen. I²C-Pull-ups nach 3,3 V müssen vorhanden sein.
SD-Karte für den Einstieg mit FAT32 formatieren, z. B. eine SDHC-Karte mit 8–32 GB.
GPIO5 ist beim klassischen ESP32 ein Strapping-Pin: Das Modul darf den Pegel beim
Booten nicht unzulässig verändern. Bei Bootproblemen einen geeigneten freien
CS-Pin wählen und `Config.h` anpassen.

INT1 und INT2 des ADXL345 werden für diese zeitgesteuerte Einstiegsversion nicht
benötigt. Die GPIO2-Verbindung aus dem alten Sketch kann entfallen. Eine
eingebaute LED auf GPIO16 wird nicht vorausgesetzt.

## Mess- und Speicherablauf

```text
ESP-Timer alle 10 ms
    → Task-Benachrichtigung
    → Messaufgabe, Priorität 3: Zeitstempel und I²C-Messwert
    → Queue mit 512 Datensätzen
    → SD-Aufgabe, Priorität 1: CSV in Blöcken schreiben, jede Sekunde flushen
```

Der ESP-Timer benutzt `ESP_TIMER_TASK`: Sein Callback läuft in einem Systemtask
und benachrichtigt nur die Messaufgabe. I²C und SD-Zugriffe stehen nicht im
Callback. FreeRTOS plant die beiden Anwendungstasks; eine Core-Bindung wird nicht
benötigt. Der Puffer überbrückt bei 100 Hz rechnerisch etwa **5,12 Sekunden**,
sofern er vorher leer ist. Er garantiert keine maximale SD-Latenz.

Der ADXL345 erzeugt intern 200 Messwerte/s. Alle nominell 10 ms wird sein aktuellster
Wert gelesen; der FIFO bleibt abgeschaltet. Diese Polling-Basis synchronisiert
sich nicht mit dem Sensortakt und speichert nicht jeden internen Sensorwert.
Der Zeitstempel markiert den Beginn des I²C-Lesens, nicht den exakten Aufnahmezeitpunkt
im Sensor. Ein verspäteter Task liest einmal und zählt zusammengefasste
Timer-Benachrichtigungen. Datenverkehr und Scheduling können Jitter verursachen;
dies ist keine Garantie für harte Echtzeit.

Bei voller Queue wird der neue Datensatz verworfen und ein Zähler erhöht. Der
nächste erfolgreich gespeicherte Datensatz enthält den aktualisierten Zähler;
bei einem Ausfall ohne Folgedatensatz kann dieser letzte Zählerstand fehlen.
Weitere verlorene Zwischenwerte im Sensor werden mit dieser Methode nicht erfasst.

Nach jedem Start entsteht eine neue Datei: `/dm00000.csv`, `/dm00001.csv`, usw.
Vorhandene Dateien werden nicht überschrieben. Die Spalten sind:

```text
t_us;sequence;skipped_timer_ticks;queue_drops;read_errors;x_g;y_g;z_g
```

`t_us` sind Mikrosekunden der monotonen ESP-Laufzeituhr, kein Datum und keine Uhrzeit.
`sequence` ist der laufende Timer-Tick-Zähler; Verlust- und Lesefehlerzähler sind kumulativ.
Beschleunigungen stehen in g mit Dezimalpunkt; sie enthalten den Schwerkraftanteil.
Es werden keine fremden Kalibrierfaktoren aus dem Original übernommen. Der
Messbereich ist zunächst ±2 g und muss bei stärkeren Stößen angepasst werden.

Fehlt beim Start Sensor oder SD-Karte, startet die Aufzeichnung nicht. Ein erkannter
SD-Schreibfehler stoppt den Timer und meldet den Fehler seriell. Ein automatischer
Wiederanlauf der SD-Aufzeichnung ist nicht enthalten. Meldet die Sensorbibliothek
einen Lesefehler, wird der Datensatz ausgelassen und `read_errors` erhöht; die
erste Störung wird seriell gemeldet. Nachfolgende gültige Datensätze enthalten den
Fehlerzähler. Ohne spätere gültige Messwerte bleibt dieser Zähler auf der SD-Karte
unsichtbar. Plausibilitätskontrollen und eine vollständige Fehlerzustandsmaschine
sind noch zu ergänzen.
`flush()` reduziert das Verlustfenster, macht das Dateisystem aber nicht stromausfallsicher.
Zum kontrollierten Entnehmen wäre noch ein Stopptaster bzw. Stoppkommando zu ergänzen.

## Befund zum Original

Geprüft: [dustmover.ino, Commit af1524e](https://github.com/buddiej/dustmover/blob/af1524e488445834498c97587a4f9d3c9db32b91/dustmover.ino).

- Der WATERMARK-Interrupt wird auf `INT_PIN_1` konfiguriert, gewartet wird über
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
Für Vibrationsmessung mit höherer Rate: DATA_READY oder FIFO-WATERMARK, konsistente
Sampleblöcke, dokumentierte Zeitbasis und Überlauferkennung verwenden. Bei hohen
ADXL345-Datenraten ist außerdem SPI statt I²C zu prüfen. Für längere Messungen
sind Dateirotation, voller Datenträger, kontrollierter Stopp und Stromausfallverhalten
zu ergänzen. Antriebssteuerung erst mit definierten Ausgängen, Zuständen und Rückmeldungen hinzufügen.

## Prüfung auf der Hardware

**Buildprüfung am 31.10.2026:** `pio run -e esp32dev` erfolgreich mit der oben
festgelegten Konfiguration. Flash: 354669 Byte; statischer RAM: 22512 Byte.
Taskstacks und Queue benötigen zusätzlich dynamischen RAM. Keine angeschlossene
Hardware und kein Uploadtest; Arduino IDE und andere Host-Betriebssysteme wurden
hier nicht gesondert getestet.

1. Ohne Sensor bzw. ohne SD starten: Fehlermeldung, keine gestartete Aufzeichnung.
2. Mit beiden starten: Dateiname und Startmeldung erscheinen; neue CSV hat Kopfzeile.
3. Sensor ruhig hinlegen: ein plausibler Schwerkraftvektor von ungefähr 1 g,
   Achsenwerte ändern sich beim Kippen.
4. Eine Minute aufzeichnen: nominell etwa 6000 Zeilen, monoton steigende Zeitstempel,
   Abstände ungefähr 10000 µs; Verlustzähler prüfen.
5. SD-Schreibstörungen und Laufzeitfehler gezielt prüfen, bevor die Firmware als
   verlässlicher Messlogger eingesetzt wird.

Diese Prüfschritte sind ein Inbetriebnahmeplan; ohne angeschlossene Hardware
lassen sich elektrische Funktion und tatsächliches Timing hier nicht bestätigen.

## Dokumentation der verwendeten Werkzeuge

- [PlatformIO ESP32-Plattform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [ESP-Timer](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/esp_timer.html)
- [FreeRTOS in ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/freertos.html)
- [ADXL345_WE](https://github.com/wollewald/ADXL345_WE)
