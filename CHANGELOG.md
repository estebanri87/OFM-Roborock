# Changelog OFM-Roborock

## 0.1.0 - 2026-10-04

Erste Fassung. Lokale Anbindung von Roborock-Saugrobotern mit der klassischen
miIO-Schnittstelle (z.B. S5), nach dem Vorbild von python-miio.

### Added
- miIO-Client (UDP 54321): Hello-Handshake, AES-128-CBC, MD5-Prüfsumme, nicht blockierend
- Kanäle nach OpenKNX-Kanalauswahl (Typ-Variante), je Roboter IP-Adresse und Token
- Befehle: Reinigung starten, Pause, Stopp, Zur Station, Roboter finden, Saugstufe
- Status: Erreichbar, Zustand (Code und Text), Reinigt, Lädt, Akku, Saugstufe, Fehler
  (Alarm, Code, Text), Fläche und Dauer der letzten Reinigung
- Verschleiß: Restlaufzeit von Hauptbürste, Seitenbürste, Filter und Sensoren, Zurücksetzen per Objekt
- Modulobjekt Diagnose
- Konsole: `rob`, `rob call <Kanal> <Methode> [JSON]`
