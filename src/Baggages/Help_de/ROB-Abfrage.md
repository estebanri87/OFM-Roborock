### Abfrage

**Zustand abfragen alle** legt fest, wie oft Zustand, Akku, Saugstufe, Fehler und die Daten
der letzten Reinigung abgefragt werden (0 bis 3600 Sekunden, Standard 10). 0 schaltet die
Abfrage ab. Empfohlen sind 5 bis 30 Sekunden.

Nach jedem Befehl über den Bus wird der Zustand sofort neu abgefragt.

Antwortet der Roboter dreimal hintereinander nicht, setzt das Modul das Objekt **Erreichbar**
auf 0.

