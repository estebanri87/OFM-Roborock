### Sendeverhalten

Alle Statusobjekte senden bei Änderung. **Statusobjekte zyklisch senden alle** sendet sie
zusätzlich in diesem Abstand erneut, z. B. für Visualisierungen (0 bis 240 Minuten,
0 = nur bei Änderung).

Die Eingangsobjekte **Reinigung starten**, **Pause**, **Stopp**, **Zur Station** und
**Roboter finden** lösen bei einer 1 aus; eine 0 wird ignoriert. **Saugstufe** nimmt die Werte
0 = Leise, 1 = Standard, 2 = Stark, 3 = Max entgegen.

