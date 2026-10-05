<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 OpenKNX -->

<!-- KEINE MARKDOWN-TABELLEN in den DOC-Bloecken: die ETS zeigt sie als rohe Pipe-Zeichen an.
     Aufzaehlungen verwenden. Die Bloecke zwischen DOC und DOCEND werden von
     "openknxproducer baggages" (VS-Code-Task "OpenKNXproducer Documentation") zu den
     Hilfetexten in src/Baggages/Help_de/ verarbeitet. -->

# Applikationsbeschreibung Roborock

Das Modul bindet **Roborock-Saugroboter** mit der klassischen **miIO-Schnittstelle** (z. B.
Roborock S5) an KNX an. Es spricht den Roboter **direkt im lokalen Netz** an (UDP-Port 54321),
ohne Xiaomi-Cloud. Gestartet, pausiert, gestoppt und zur Station geschickt wird über
Eingangsobjekte; Zustand, Akku, Fehler, die letzte Reinigung und der Verschleiß von Bürsten,
Filter und Sensoren kommen als Statusobjekte.

Voraussetzungen:

* IP-Adresse und **Token** des Roboters (siehe [Token](#token))
* ein Gerät mit **ESP32** (z. B. REG1-LAN-TP-Base). Auf RP2040-Geräten ist das Modul nicht
  enthalten.

Die Mi-Home-App und eine Home-Assistant-Integration (z. B. *Xiaomi Miot*) funktionieren parallel
weiter.

## Wichtige Hinweise

* Diese KNXprod wird nicht von der KNX Association offiziell unterstützt!
* Die Erzeugung der KNXprod geschieht auf eure eigene Verantwortung!

# Allgemein

<!-- DOC -->
## Allgemein

Das Modul ist standardmäßig aktiv. Wird es nicht gebraucht, lässt es sich unter
**OpenKNX → Module** abschalten.

Je Roboter wird auf der Seite **Kanalauswahl** ein Kanal angelegt.

Auf der Konsole zeigt `rob` den Zustand aller Kanäle; `rob call 1 get_status` sendet einen
beliebigen miIO-Befehl an Kanal 1 und schreibt die Antwort ins Log.

<!-- DOCEND -->

<!-- DOC -->
## Diagnose

Das Modul hat ein einziges Objekt, das nicht zu einem Kanal gehört:
**Diagnose-Meldungstext**. Es meldet als kurzen Text, wenn ein Kanal erreichbar oder nicht mehr
erreichbar wird („K1 online", „K1 offline") oder wenn der Token nicht passt („K1 Token?").

Das Objekt ist ab Werk **abgeschaltet**, damit das Modul ohne eingerichteten Roboter kein
Kommunikationsobjekt belegt. Die Kanalobjekte erscheinen ohnehin erst, wenn in der
**Kanalauswahl** ein Kanaltyp gesetzt ist.

<!-- DOCEND -->

# Kanäle

<!-- DOC -->
## Kanaltyp

Ein Kanal bildet einen Saugroboter ab. Kanäle werden auf der Seite **Kanalauswahl** aktiviert,
indem dort der Kanaltyp gewählt wird; „Deaktiviert" entfernt den Kanal. Auf der Seite des
Kanals lässt sich der Typ wechseln, aber nicht deaktivieren.

**Roborock (miIO lokal)**: Roborock-Modelle mit der klassischen Schnittstelle, wie sie auch
python-miio als „roborock.vacuum" anspricht (z. B. S5, S50, S6). Neuere Modelle, die nur noch
über die Roborock-App laufen, werden nicht unterstützt.

<!-- DOCEND -->

<!-- DOC -->
## Verbindung

**IP-Adresse** des Roboters im lokalen Netz, z. B. `192.168.1.50`. Ein Hostname ist nicht
möglich.

Die Adresse steht in der Geräteliste des Routers; die beim **Token** genannten Werkzeuge
zeigen sie ebenfalls an. Der Roboter sollte im Router eine feste Adresse bekommen,
sonst bricht die Verbindung nach einem Adresswechsel ab.

Die Verbindung läuft über UDP-Port 54321; das KNX-Gerät und der Roboter müssen sich direkt
erreichen können (gleiches Netz oder entsprechend geroutet).

<!-- DOCEND -->

<!-- DOC -->
## Token

Der **Token** ist ein 32-stelliger Hex-Schlüssel, mit dem jede Nachricht an den Roboter
verschlüsselt wird. Er wird beim Koppeln vergeben und liegt im Roboter sowie im Mi-Konto.
Ohne den richtigen Token antwortet der Roboter nicht bzw. die Antwort lässt sich nicht
entschlüsseln; das Diagnoseobjekt meldet dann „K1 Token?".

In der Mi-Home-App wird der Token **nicht** angezeigt. Er wird einmalig über das Mi-Konto
ausgelesen; danach wird die Cloud nicht mehr gebraucht. Drei Wege:

* **Xiaomi Cloud Tokens Extractor** (der übliche Weg, ohne weitere Software): das Programm von
  github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor herunterladen und starten, mit den
  Zugangsdaten des Mi-Kontos anmelden und als Server die Region wählen, in der das Gerät in der
  App liegt (z. B. „de" für Europa, „sg" für Singapur). Es listet alle Geräte des Kontos mit
  Namen, IP-Adresse und Token auf.
* **Home Assistant** mit der Integration *Xiaomi Miot* (al-one): unter
  **Entwicklerwerkzeuge → Aktionen** die Aktion **Xiaomi Miot: Get Token** aufrufen und im Feld
  `name` einen Teil des Gerätenamens eintragen. Das Ergebnis enthält Token und IP-Adresse.
* **python-miio**: `miiocli cloud` meldet sich am Mi-Konto an und listet Geräte mit Token auf.

Der Token besteht nur aus den Zeichen 0-9 und a-f. Die ETS nimmt ihn erst an, wenn genau 32
solcher Zeichen eingetragen sind.

Der Token ändert sich, wenn der Roboter in der Mi-Home-App zurückgesetzt oder neu gekoppelt
wird. Dann muss er hier neu eingetragen werden.

<!-- DOCEND -->

<!-- DOC -->
## Abfrage

**Zustand abfragen alle** legt fest, wie oft Zustand, Akku, Saugstufe, Fehler und die Daten
der letzten Reinigung abgefragt werden (0 bis 3600 Sekunden, Standard 10). 0 schaltet die
Abfrage ab. Empfohlen sind 5 bis 30 Sekunden.

Nach jedem Befehl über den Bus wird der Zustand sofort neu abgefragt.

Antwortet der Roboter dreimal hintereinander nicht, setzt das Modul das Objekt **Erreichbar**
auf 0.

<!-- DOCEND -->

<!-- DOC -->
## Verschleiß

**Verschleiß abfragen alle** legt fest, wie oft die Restlaufzeit von Hauptbürste,
Seitenbürste, Filter und Sensoren abgefragt wird (0 bis 240 Minuten, Standard 60). 0 schaltet
die Abfrage ab.

Die Restlaufzeit wird in Prozent gesendet, bezogen auf die Lebensdauer, die auch die
Mi-Home-App verwendet: Hauptbürste 300 h, Seitenbürste 200 h, Filter 150 h, Sensoren 30 h.

Über das Objekt **Verschleiß zurücksetzen** wird nach dem Tausch bzw. Reinigen der Zähler
zurückgesetzt: 1 = Hauptbürste, 2 = Seitenbürste, 3 = Filter, 4 = Sensoren.

<!-- DOCEND -->

<!-- DOC -->
## Sendeverhalten

Alle Statusobjekte senden bei Änderung. **Statusobjekte zyklisch senden alle** sendet sie
zusätzlich in diesem Abstand erneut, z. B. für Visualisierungen (0 bis 240 Minuten,
0 = nur bei Änderung).

Die Eingangsobjekte **Reinigung starten**, **Pause**, **Stopp**, **Zur Station** und
**Roboter finden** lösen bei einer 1 aus; eine 0 wird ignoriert. **Saugstufe** nimmt die Werte
0 = Leise, 1 = Standard, 2 = Stark, 3 = Max entgegen.

<!-- DOCEND -->

## Objekte je Kanal

Eingänge:

* **Reinigung starten**, **Pause**, **Stopp**, **Zur Station**, **Roboter finden** (DPT 1.001,
  Auslöser bei 1)
* **Saugstufe** (DPT 5.010): 0 = Leise, 1 = Standard, 2 = Stark, 3 = Max
* **Verschleiß zurücksetzen** (DPT 5.010): 1 = Hauptbürste, 2 = Seitenbürste, 3 = Filter,
  4 = Sensoren

Ausgänge:

* **Erreichbar** (DPT 1.011)
* **Status Saugstufe** (DPT 5.010), gleiche Werte wie der Eingang
* **Zustand** als Code (DPT 5.010) und **Zustand Text** (DPT 16.001), z. B. „Reinigt",
  „Lädt", „Fährt zurück"
* **Reinigt**, **Lädt** (DPT 1.011)
* **Akku** (DPT 5.001)
* **Fehler** (DPT 1.005), **Fehlercode** (DPT 5.010), **Fehlertext** (DPT 16.001)
* **Gereinigte Fläche** in m² (DPT 14.010) und **Reinigungsdauer** in Minuten (DPT 7.006) der
  letzten bzw. laufenden Reinigung
* **Hauptbürste**, **Seitenbürste**, **Filter**, **Sensoren Restlaufzeit** (DPT 5.001)
