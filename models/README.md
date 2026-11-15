# models/

Aquí se descargan los pesos de los modelos y las voces. **Ningún peso se sube al repositorio**
(`.gitignore` los excluye y la CI falla si alguno aparece en git).

- `manifest.json`: nombre exacto, revisión, URL oficial, cuantización, licencia, tamaño y SHA-256
  de cada archivo.
- Para descargar: `bash scripts/descargar_modelos.sh base` (o `alto`).

Si un SHA-256 figura como `PENDIENTE`, el script lo calcula y lo muestra: verifíquelo contra la
fuente oficial y regístrelo en `manifest.json` mediante un pull request.
