# Release v0.1.0 — Rilascio Iniziale

Rilascio iniziale della toolchain "raylib-map".

Contenuto principale:
- Editor di tilemap con interfaccia grafica per creare e modificare mappe.
- Esportatore in C (`my_map.c`) per utilizzare le mappe nel runtime standalone.
- Runtime minimale per caricare/eseguire le mappe esportate e muovere un personaggio.

I pacchetti costruiti includono binari e risorse per macOS, Windows e Linux quando il workflow GitHub Actions viene eseguito su tag di rilascio.

Note tecniche:
- Requisiti: `cmake`, `pkg-config`, `raylib` (o il workflow compila raylib automaticamente)
- Per creare manualmente il pacchetto su macOS:
  ```bash
  cmake -S . -B build-rmp -DCMAKE_BUILD_TYPE=Release
  cmake --build build-rmp --config Release --target package
  ```

Grazie per aver usato raylib-map!
