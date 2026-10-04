### Allgemein

Das Modul ist standardmäßig aktiv. Wird es nicht gebraucht, lässt es sich unter
**OpenKNX → Module** abschalten.

Je Roboter wird auf der Seite **Kanalauswahl** ein Kanal angelegt.

Das Objekt **Diagnose** meldet als kurzen Text, wenn ein Kanal erreichbar oder nicht mehr
erreichbar wird („K1 online", „K1 offline") oder wenn der Token nicht passt („K1 Token?").

Auf der Konsole zeigt `rob` den Zustand aller Kanäle; `rob call 1 get_status` sendet einen
beliebigen miIO-Befehl an Kanal 1 und schreibt die Antwort ins Log.

