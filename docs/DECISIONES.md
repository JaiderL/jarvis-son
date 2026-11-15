# Registro de decisiones técnicas

Cada decisión indica el contexto, lo que se eligió, por qué y cómo se verificó.
"✅ Linux" = verificado en Linux x86_64 (CI o contenedor Ubuntu 24.04). "📱" = falta validar en
el teléfono real.

---

## D1. Procesos de una sola ejecución, nunca servidores residentes

- **Decisión:** cada etapa lanza un binario (`whisper-cli`, `llama-completion`,
  `sherpa-onnx-offline-tts`) que carga su modelo, responde y termina.
- **Por qué:** requisito del docente y del teléfono de 3 GB: nunca dos modelos en memoria a la
  vez. El costo es recargar el modelo en cada turno (≈0,5–1 s en Linux; 📱 medir en el teléfono).
- **Verificación:** ✅ Linux, memoria pico por proceso con `wait4` (`ru_maxrss`): whisper ~149 MB,
  llama ~941 MB, sherpa ~167 MB, uno a la vez.

## D2. Lanzador propio con `fork` + `execvp` (sin `system`/`popen`)

- **Decisión:** `jarvis::ejecutar()` en `src/proceso.cpp`: `fork`, `execvp` con argv fijo,
  `poll` sobre tubos de stdout/stderr, tiempo límite, grupo de procesos propio y `wait4`.
- **Por qué:** requisito de seguridad; además permite medir memoria pico y distinguir "binario no
  encontrado" (127), tiempo límite (124), señal (128+N) y Ctrl+C (130).
- **Garantías:** la CI falla si aparece `system(`, `popen(` o `execl*(` en `src/` o `include/`;
  `tests/unit/prueba_proceso.cpp` comprueba que metacaracteres de shell llegan literales.

## D3. llama.cpp: `llama-completion -no-cnv` con prompt ChatML en archivo y `<think></think>` vacío

- **Contexto:** el plan pedía desactivar el modo "thinking" de Qwen3.5 con el flag correcto para
  el binario de una sola ejecución. En llama.cpp **v1.0.0** el binario de una ejecución es
  `llama-completion` (`llama-cli` pasó a ser la interfaz interactiva) y existen los flags
  `-rea/--reasoning [on|off|auto]` y `--reasoning-budget N`.
- **Lo que se probó (✅ Linux, llama.cpp v1.0.0, Qwen3.5-0.8B Q4_0):**
  - `-cnv -st --jinja -sys ... -p ...` → **aborta** (señal 6): la plantilla Jinja de Qwen3.5 lanza
    `No user query found in messages` al formatear el ejemplo de chat.
  - `-cnv -st --jinja -rea off -p ...` (sin `-sys`) → responde, pero **sigue razonando** (en inglés)
    antes de `</think>`: `-rea off` no lo suprime en este modo.
  - `-no-cnv -f prompt.txt` con el prompt ChatML terminado en
    `<|im_start|>assistant\n<think>\n\n</think>\n\n` → responde **directo y en español**, ~1,2 s.
- **Decisión:** usar la tercera opción. Es exactamente lo que produce la plantilla oficial de Qwen
  con `enable_thinking=false`, no depende de flags que cambian entre versiones y el texto del
  usuario va **por archivo** (`-f`). Se neutralizan marcas `<|...|>` y `<think>` en el texto del
  usuario para que no altere el prompt. Además, `limpiar_respuesta_slm()` descarta cualquier
  razonamiento residual (`<think>…</think>` o un `</think>` sin apertura).
- **Configurable:** `slm.args_extra_llama` en `config.json` permite agregar flags validados si una
  versión futura lo requiere, sin tocar el código.

## D4. GGUF de Qwen3.5-0.8B: repositorio `ggml-org`

- **Contexto:** Qwen no publica GGUF propios de Qwen3.5 en Hugging Face (solo safetensors, FP8 y
  GPTQ). La conversión Q4_0 de **ggml-org** (la organización de llama.cpp) pesa 563 036 064 bytes
  (~563 MB), coincide con el tamaño del plan y declara `base_model: Qwen/Qwen3.5-0.8B` y licencia
  Apache-2.0.
- **Decisión:** `ggml-org/Qwen3.5-0.8B-GGUF`, archivo `Qwen3.5-0.8B-Q4_0.gguf`, fijado a la
  revisión `004fd086…` con SHA-256 verificado.

## D5. Perfil alto: Qwen3.5-2B Q4_K_M de Unsloth

- **Contexto:** ni Qwen ni ggml-org publican GGUF de Qwen3.5-2B. El docente permite un LLM
  alternativo en el teléfono mejor.
- **Decisión:** `unsloth/Qwen3.5-2B-GGUF` (`Qwen3.5-2B-Q4_K_M.gguf`, 1,28 GB, Apache-2.0),
  revisión fijada y SHA-256 del índice LFS de Hugging Face. 📱 Validar velocidad y memoria.

## D6. Whisper: pesos de `ggerganov/whisper.cpp` en Hugging Face

- **Contexto:** el código está en `github.com/ggml-org/whisper.cpp`, pero su script oficial
  `download-ggml-model.sh` descarga los pesos de `huggingface.co/ggerganov/whisper.cpp`.
- **Decisión:** `ggml-tiny.bin` (base) y `ggml-base.bin` (alto), **multilingües**, fijados a la
  revisión `5359861c…`. El orquestador siempre pasa `-l es` y **rechaza** modelos `*.en.*`.

## D7. sherpa-onnx v1.13.8 compilado desde fuente (no v2.x)

- **Contexto:** sherpa-onnx v2.0.0 eliminó el frontend espeak-ng del binario: `--vits-data-dir`
  se ignora y las voces Piper fallan con `phoneme_codepoints is empty ... no frontend available`
  (✅ reproducido en Linux con v2.0.1).
- **Decisión:** fijar **v1.13.8**, la última 1.x, que conserva espeak-ng (✅ Linux: la voz
  `es_MX-ald-medium` sintetiza correctamente).
- **Cómo obtenerlo en Termux (investigación):**
  - No existe paquete `pkg install sherpa-onnx` en Termux.
  - Los binarios precompilados `linux-aarch64` de k2-fsa enlazan contra **glibc**; Termux usa
    **Bionic**, así que no son una opción confiable.
  - **Elegido:** compilar en Termux con CMake (`instalar.sh`), solo el objetivo
    `sherpa-onnx-offline-tts`, `BUILD_SHARED_LIBS=ON`, sin Python/tests/portaudio/websocket. Para
    onnxruntime se usa el paquete `onnxruntime` de Termux si existe; si no, el CMake de sherpa-onnx
    detecta `CMAKE_SYSTEM_NAME=Android` y descarga el onnxruntime oficial para Android arm64.
  - 📱 Validar que la compilación termine en el teléfono de 3 GB con `-j2` (o `JOBS=1`).

## D8. Voces es_MX: `ald` (medium) para base y `claude` (high) para alto

- **Decisión:** paquetes de k2-fsa (`vits-piper-es_MX-*.tar.bz2`, release `tts-models`), que traen
  el `.onnx` con metadatos para sherpa-onnx, `tokens.txt` y `espeak-ng-data/`. Base usa *medium*
  (más rápida); alto usa *high* (mejor calidad).
- **SHA-256:** k2-fsa no publica el hash de estos paquetes, así que el manifiesto dice
  `PENDIENTE` y el script lo calcula. Valor calculado el 2026-11-15 para
  `vits-piper-es_MX-ald-medium.tar.bz2`:
  `c60cf7bc853eb01a875fba0756523fecf9d9f46c763b19785d223fc981fb4fb6` (no se registró en el
  manifiesto porque no hay fuente oficial para contrastarlo).

## D9. Reproducción con `play-audio`

- **Decisión:** `play-audio` (paquete de Termux, OpenSL ES) reproduce el WAV de forma **síncrona**,
  así la etapa TTS termina cuando termina el audio. `termux-media-player` es asíncrono y requeriría
  esperar por la duración. 📱 Validar.

## D10. Grabación de 6 s y validación 5–8 s

- `termux-microphone-record -l 6` (AAC), conversión con FFmpeg a PCM s16le mono 16 kHz recortada a
  8 s, y validación de formato, duración `[5, 8] s` y energía (RMS ≥ 50) para detectar grabaciones
  vacías. El límite de 6 s deja margen a la latencia de arranque del micrófono. 📱 Ajustar
  `grabacion.segundos` si el teléfono corta antes.

## D11. Versiones fijadas

| Componente | Versión | Motivo |
|---|---|---|
| whisper.cpp | v1.9.5 | Última estable al 2026-11-15 |
| llama.cpp | v1.0.0 | Última estable al 2026-11-15; contiene `tools/completion` |
| sherpa-onnx | v1.13.8 | Ver D7 |

## D12. Otras decisiones tomadas por el equipo de desarrollo

- **Licencia del código:** MIT (simple y compatible con las licencias de los modelos).
- **Lector JSON propio** (`src/json.cpp`) para no depender de librerías externas en Termux.
- **Métricas privadas:** `logs/metricas.csv` solo tiene números y estados; la CI de integración
  verifica que no aparezca el texto de la conversación.
- **Calidad observada del 0.8B (✅ Linux):** responde breve y en español, pero comete errores
  factuales y aritméticos (p. ej. "12 / 8 = 1.5" para "doce por ocho"). Es una limitación del
  modelo, no del orquestador; documentarla en el informe.

## D13. Respuestas rápidas en C++ antes del SLM

- **Contexto:** cargar Qwen cuesta ~940 MB de RAM y varios segundos por turno, aunque la frase sea
  "hola". Además Qwen no conoce la hora ni la fecha y las inventaría.
- **Decisión:** después de STT, `buscar_atajo()` (`src/atajos.cpp`) compara la transcripción
  normalizada (minúsculas, sin tildes ni signos) con reglas de `config.json`. Si coincide, el turno
  salta **STT → TTS** sin cargar el SLM. La hora y la fecha se calculan con el reloj del teléfono.
- **Restricciones para no responder mal:**
  - Solo frases de hasta `max_palabras` (6). "Hola, explícame la fotosíntesis" va a Qwen.
  - Por defecto, reglas `exacta`. La tolerancia acepta 1 error por palabra solo en palabras de
    4+ letras ("ola" ≈ "hola", pero "de" ≠ "que").
  - `temas_bloqueados` se revisa siempre y responde un texto fijo (contraseñas, datos bancarios).
- **Privacidad:** "repite" usa la última respuesta guardada **solo en memoria**. El CSV registra
  la columna `ruta` (`slm`, `atajo`, `bloqueo`), no el texto ni el id de la regla.
- **Compatibilidad:** si `config.json` no tiene la sección, los atajos quedan desactivados. Si
  `logs/metricas.csv` tiene el encabezado anterior, se renombra a `metricas.csv.anterior`.
- **Verificación:** ✅ Linux: pruebas unitarias (normalización, tolerancia, rotación, hora/fecha,
  temas bloqueados, configuración inválida) e integración (el "SLM" es un binario que falla, y el
  turno con "hola" igual termina bien porque nunca lo ejecuta). 📱 Medir el ahorro real en el
  teléfono comparando `ms_total` por ruta en `prueba_aceptacion.sh`.
