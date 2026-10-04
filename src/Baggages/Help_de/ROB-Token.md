### Token

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

