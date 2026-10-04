# OFM-Roborock

OpenKNX-Modul zur **lokalen** Anbindung von Roborock-Saugrobotern mit der klassischen
miIO-Schnittstelle (z.B. Roborock S5) an den KNX-Bus. Kommunikation direkt im Heimnetz über
UDP-Port 54321 — **keine Cloud**. Die Mi-Home-App und eine Home-Assistant-Integration laufen
parallel weiter.

> **Status: Beta.** Das Protokoll ist gegen [python-miio](https://github.com/rytilahti/python-miio)
> gegengeprüft, die ESP32-Firmware baut. Der Test am realen Gerät steht noch aus.

## Voraussetzungen

- **IP-Adresse** des Roboters, am besten im Router fest vergeben.
- **Token** des Roboters, 32 Hex-Zeichen. Die Mi-Home-App zeigt ihn nicht an; er wird einmalig
  über das Mi-Konto ausgelesen — mit dem
  [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor),
  in Home Assistant mit der Aktion `xiaomi_miot.get_token` der Integration *Xiaomi Miot* oder
  mit `miiocli cloud`. Einzelheiten in der
  [Applikationsbeschreibung](doc/Applikationsbeschreibung-Roborock.md).
- **ESP32**, z.B. REG1-LAN-TP-Base. Auf RP2040-Geräten ist das Modul nicht enthalten: miIO
  verschlüsselt jedes Paket mit AES-128-CBC und signiert es mit MD5, beides kommt aus mbedTLS.

## Funktionen

Ein Kanal entspricht einem Roboter. Kanäle werden nach OpenKNX-Standard über die
**Kanalauswahl** aktiviert (Typ-Variante mit „Deaktiviert").

### Befehle (KNX → Roboter)

| Objekt | DPT | Wirkung |
|---|---|---|
| Reinigung starten, Pause, Stopp, Zur Station, Roboter finden | 1.001 | Auslöser bei 1 |
| Saugstufe | 5.010 | 0 = Leise, 1 = Standard, 2 = Stark, 3 = Max |
| Verschleiß zurücksetzen | 5.010 | 1 = Hauptbürste, 2 = Seitenbürste, 3 = Filter, 4 = Sensoren |

### Status (Roboter → KNX)

- **Erreichbar** (1.011), **Zustand** als Code (5.010) und als Text (16.001), **Reinigt** und
  **Lädt** (1.011), **Akku** (5.001), **Status Saugstufe** (5.010)
- **Fehler** (1.005), **Fehlercode** (5.010) und **Fehlertext** (16.001)
- **Gereinigte Fläche** in m² (14.010) und **Reinigungsdauer** in Minuten (7.006)
- **Restlaufzeit** von Hauptbürste, Seitenbürste, Filter und Sensoren (5.001)
- Modulobjekt **Diagnose** (16.001)

Alle Statusobjekte senden bei Änderung, auf Wunsch zusätzlich zyklisch.

## Technik

`MiioClient` ist bewusst **KNX-frei** gehalten (Muster aus OFM-SolarmanPV) und blockiert nie:
nicht-blockierendes UDP-Socket, Zeitlimits ausschließlich über `millis()`, feste Puffer, nur
IPv4-Literale — eine Namensauflösung würde die Firmware anhalten.

Ablauf eines Aufrufs: Hello-Paket, Antwort mit Geräte-ID und Zeitstempel, dann die
JSON-RPC-Anfrage mit AES-128-CBC (Schlüssel `MD5(Token)`, IV `MD5(Schlüssel+Token)`) und
MD5-Prüfsumme im Kopf.

## Konsole

```
rob                                    Status aller Kanäle
rob call <Kanal> <Methode> [JSON]      beliebigen miIO-Befehl senden, z.B. 'rob call 1 get_status'
```

`rob call` ist auch der Weg, Funktionen auszuprobieren, die das Modul noch nicht als Objekt
anbietet, etwa die Raumreinigung (`app_segment_clean`).

## Dokumentation

- [Applikationsbeschreibung](doc/Applikationsbeschreibung-Roborock.md) — zugleich die Quelle
  der ETS-Hilfetexte in `src/Baggages/Help_de`, erzeugt vom VS-Code-Task
  *OpenKNXproducer Documentation*
- [CHANGELOG](CHANGELOG.md)
