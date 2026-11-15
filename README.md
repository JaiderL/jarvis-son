# Jarvis-son V1

```
MIC ──► STT ──► SLM ──► TTS
termux-microphone-record + FFmpeg │ whisper.cpp (tiny, -l es) │ llama.cpp (Qwen3.5-0.8B Q4_0) │ sherpa-onnx (Piper es_MX)
```

- **Sin interrupciones:** cada etapa termina antes de iniciar la siguiente.
- **Un modelo en memoria a la vez:** cada etapa es un proceso hijo que carga su modelo, responde
  y sale. No hay servidores residentes.
- **Pensado para 3 GB de RAM** (perfil `base`); perfil `alto` para teléfonos mejores.
- **Funciona en modo avión** una vez descargados los modelos.
- **Seguro:** sin `system()` ni `popen()`; los procesos se lanzan con `fork` + `execvp` y argv
  fijo. El texto del usuario va por archivo o como argumento separado, nunca por un shell.
- **Respuestas rápidas:** "hola", "¿qué hora es?", "gracias" o "adiós" se responden en C++ sin
  cargar Qwen (configurable en `config.json`, sección `respuestas_rapidas`).
- **Privado:** `logs/metricas.csv` guarda solo tiempos y memoria; nunca audio ni texto.

## Inicio rápido (en Termux)

```bash
git clone https://github.com/JaiderL/jarvis-son.git ~/jarvis-son
```

```bash
cd ~/jarvis-son && bash scripts/instalar.sh
```

```bash
bash scripts/descargar_modelos.sh base
```

```bash
./build/jarvis
```

La guía completa, desde cero (Termux, Termux:API, SSH con PuTTY/WinSCP, pruebas, modo avión y
problemas comunes), está en **[docs/INSTALACION.md](docs/INSTALACION.md)**.

## Estructura

| Ruta | Contenido |
|---|---|
| `src/`, `include/jarvis/` | Orquestador C++17: estados, lanzador de procesos, config, métricas, etapas |
| `scripts/` | `instalar.sh`, `descargar_modelos.sh`, `probar_*.sh`, `prueba_aceptacion.sh` |
| `config/config.json` | Perfiles `base` y `alto` |
| `models/manifest.json` | Metadatos de modelos (URL, revisión, licencia, tamaño, SHA-256). **Sin pesos** |
| `tests/` | Pruebas unitarias, integración con binarios falsos, WER y reporte de latencias |
| `docs/` | [INSTALACION](docs/INSTALACION.md), [ARQUITECTURA](docs/ARQUITECTURA.md), [DECISIONES](docs/DECISIONES.md) |

## Perfiles

| Perfil | STT | SLM | Contexto | Salida | Hilos | Voz |
|---|---|---|---|---|---|---|
| `base` (3 GB) | whisper tiny | Qwen3.5-0.8B Q4_0 | 1024 | 96 | 4 | es_MX ald (medium) |
| `alto` | whisper base | Qwen3.5-2B Q4_K_M | 2048 | 128 | 6 | es_MX claude (high) |

## Estado de verificación

| Qué | Dónde se verificó |
|---|---|
| Compilación C++17 (clang y gcc, `-Werror`), pruebas unitarias, integración con binarios falsos, formato, shellcheck | CI en Linux (GitHub Actions) |
| `instalar.sh` y `descargar_modelos.sh base`; STT + SLM + TTS reales del perfil `base` sin red | Contenedor Ubuntu 24.04 x86_64 (no en ARM) |
| Captura con Termux:API, rendimiento en 3 GB, compilación en Termux, sherpa-onnx en Android, modo avión, perfil `alto` | **Pendiente: validar en el teléfono real** (ver [docs/INSTALACION.md](docs/INSTALACION.md)) |

## Licencia

Código bajo licencia MIT ([LICENSE](LICENSE)). Los modelos tienen sus propias licencias
(ver `models/manifest.json`).
