### Token

Der **Token** ist ein 32-stelliger Hex-Schlüssel, mit dem jede Nachricht an den Roboter
verschlüsselt wird. Ohne den richtigen Token antwortet der Roboter nicht bzw. die Antwort lässt
sich nicht entschlüsseln; das Diagnoseobjekt meldet dann „K1 Token?".

Den Token liest man einmalig über das Mi-Konto aus, z. B. mit dem *Xiaomi Cloud Tokens
Extractor* oder mit python-miio (`miiocli cloud`). Danach wird die Cloud nicht mehr gebraucht.

Der Token ändert sich, wenn der Roboter in der Mi-Home-App zurückgesetzt oder neu gekoppelt
wird. Dann muss er hier neu eingetragen werden.

