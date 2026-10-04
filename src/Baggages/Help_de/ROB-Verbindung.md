### Verbindung

**IP-Adresse** des Roboters im lokalen Netz, z. B. `192.168.1.50`. Ein Hostname ist nicht
möglich.

Die Adresse steht in der Geräteliste des Routers; die beim **Token** genannten Werkzeuge
zeigen sie ebenfalls an. Der Roboter sollte im Router eine feste Adresse bekommen,
sonst bricht die Verbindung nach einem Adresswechsel ab.

Die Verbindung läuft über UDP-Port 54321; das KNX-Gerät und der Roboter müssen sich direkt
erreichen können (gleiches Netz oder entsprechend geroutet).

