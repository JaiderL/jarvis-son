#!/usr/bin/env bash
# Prueba de integración del orquestador con binarios FALSOS (sin modelos ni
# teléfono). Verifica en CI la máquina de estados completa
# LISTO → GRABANDO → STT → SLM → TTS → LISTO, el manejo de ERROR, las métricas
# y que los temporales se borran. Requiere ffmpeg en el PATH.
#
# Uso: bash tests/integracion/prueba_flujo.sh RUTA/AL/BINARIO/jarvis
set -euo pipefail

JARVIS="${1:?Uso: prueba_flujo.sh RUTA/A/jarvis}"
JARVIS="$(cd "$(dirname "$JARVIS")" && pwd)/$(basename "$JARVIS")"

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "OMITIDA: ffmpeg no está instalado" >&2
    exit 0
fi

TRABAJO="$(mktemp -d)"
export TMPDIR="$TRABAJO/tmp"
mkdir -p "$TMPDIR" "$TRABAJO/config" "$TRABAJO/falsos" "$TRABAJO/models/voz/espeak-ng-data"
trap 'rm -rf "$TRABAJO"' EXIT

fallo() { echo "FALLO: $*" >&2; exit 1; }

# --- Binarios falsos -------------------------------------------------------
cat > "$TRABAJO/falsos/whisper-cli" <<'EOF'
#!/usr/bin/env bash
# Simula whisper-cli: exige -l es y escribe <of>.txt
of=""; idioma=""
while [ $# -gt 0 ]; do
  case "$1" in -of) of="$2"; shift ;; -l) idioma="$2"; shift ;; esac; shift
done
[ "$idioma" = "es" ] || { echo "idioma incorrecto" >&2; exit 9; }
# FALSO_TRANSCRIPCION permite a cada caso elegir lo que "dijo" el usuario.
if [ -n "${FALSO_TRANSCRIPCION:-}" ]; then
  printf '%s\n' "$FALSO_TRANSCRIPCION" > "$of.txt"
else
  printf ' [BLANK_AUDIO] ¿Qué hora es; rm -rf ~ && `id` $HOME?\n' > "$of.txt"
fi
EOF

cat > "$TRABAJO/falsos/llama-completion" <<'EOF'
#!/usr/bin/env bash
# Simula llama-completion: lee el prompt ChatML desde -f (archivo) en modo -no-cnv.
f=""; nocnv=0
while [ $# -gt 0 ]; do
  case "$1" in -f) f="$2"; shift ;; -no-cnv) nocnv=1 ;; esac; shift
done
[ "$nocnv" = "1" ] || { echo "falta -no-cnv" >&2; exit 9; }
[ -f "$f" ] || { echo "sin archivo de prompt" >&2; exit 8; }
grep -q 'Qué hora es' "$f" || exit 7
# El prompt debe terminar con el bloque de razonamiento vacío (thinking desactivado).
[ "$(tail -c 41 "$f" | tr -d '\n')" = "<|im_start|>assistant<think></think>" ] || { echo "thinking activo" >&2; exit 6; }
printf 'Son las tres de la tarde. [end of text]\n'
EOF

cat > "$TRABAJO/falsos/sherpa-onnx-offline-tts" <<'EOF'
#!/usr/bin/env bash
# Simula sherpa-onnx: genera un WAV corto con ffmpeg.
out=""
for a in "$@"; do case "$a" in --output-filename=*) out="${a#--output-filename=}" ;; esac; done
texto="${*: -1}"
esperado="${FALSO_TTS_ESPERADO:-Son las tres de la tarde.}"
[ "$texto" = "$esperado" ] || { echo "texto inesperado: $texto" >&2; exit 9; }
ffmpeg -hide_banner -loglevel error -nostdin -y -f lavfi -i "sine=frequency=300:duration=1" \
  -ac 1 -ar 22050 -c:a pcm_s16le "$out"
EOF

cat > "$TRABAJO/falsos/play-audio" <<'EOF'
#!/usr/bin/env bash
[ -f "$1" ] || exit 3
EOF

cat > "$TRABAJO/falsos/falla" <<'EOF'
#!/usr/bin/env bash
echo "error simulado" >&2
exit 42
EOF
chmod +x "$TRABAJO/falsos/"*

touch "$TRABAJO/models/ggml-tiny.bin" "$TRABAJO/models/qwen.gguf" "$TRABAJO/models/voz/voz.onnx" \
      "$TRABAJO/models/voz/tokens.txt"

escribir_config() {
    local slm_bin="$1"
    cat > "$TRABAJO/config/config.json" <<EOF
{
  "version": 1,
  "perfil_activo": "base",
  "binarios": {
    "whisper": "falsos/whisper-cli",
    "llama": "falsos/$slm_bin",
    "sherpa_tts": "falsos/sherpa-onnx-offline-tts",
    "ffmpeg": "ffmpeg",
    "grabar": "falsos/falla",
    "reproducir": "falsos/play-audio",
    "tts_respaldo": "falsos/falla"
  },
  "grabacion": {"segundos": 6, "duracion_min_s": 5.0, "duracion_max_s": 8.0,
                "rms_minimo": 50.0, "timeout_termux_s": 5},
  "tiempos_limite_s": {"conversion": 30, "stt": 30, "slm": 30, "tts": 30, "reproduccion": 30},
  "slm": {"temperatura": 0.3, "prompt_sistema": "Responde breve.",
          "args_extra_llama": []},
  "tts": {"motor": "sherpa", "permitir_respaldo_termux": false},
  "respuestas_rapidas": {"activado": true, "max_palabras": 6, "tolerancia_errores": true,
    "reglas": [
      {"id": "saludo", "modo": "exacta", "frases": ["hola como estas"],
       "respuestas": ["Muy bien, gracias."]},
      {"id": "despedida", "modo": "exacta", "frases": ["adios"],
       "respuestas": ["Hasta luego."], "terminar": true}],
    "temas_bloqueados": {"frases": ["contraseña"], "respuesta": "No manejo contraseñas."}},
  "metricas": {"archivo": "logs/metricas.csv"},
  "perfiles": {"base": {"modelo_stt": "models/ggml-tiny.bin", "stt_beam": 1,
               "modelo_slm": "models/qwen.gguf", "contexto": 1024, "salida_tokens": 96,
               "hilos": 4, "voz_dir": "models/voz", "voz_modelo": "voz.onnx"}}
}
EOF
}

# Audio de entrada: 6 s de tono a 44.1 kHz estéreo (el orquestador lo normaliza).
ENTRADA="$TRABAJO/entrada.wav"
ffmpeg -hide_banner -loglevel error -nostdin -y -f lavfi -i "sine=frequency=440:duration=6" \
    -ac 2 -ar 44100 "$ENTRADA"

# --- Caso 1: turno completo exitoso -----------------------------------------
escribir_config llama-completion
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 2 --auto --wav "$ENTRADA" 2>&1)" \
    || { echo "$SALIDA"; fallo "el turno exitoso devolvió error"; }
echo "$SALIDA"
for estado in "[GRABANDO]" "[STT]" "[SLM]" "[TTS]" "[LISTO]"; do
    grep -qF "$estado" <<<"$SALIDA" || fallo "no apareció el estado $estado"
done
grep -qF "Jarvis: Son las tres de la tarde." <<<"$SALIDA" || fallo "respuesta no limpiada"
grep -qF 'Usted: ¿Qué hora es; rm -rf ~ && `id` $HOME?' <<<"$SALIDA" || fallo "transcripción alterada"

CSV="$TRABAJO/logs/metricas.csv"
[ -f "$CSV" ] || fallo "no se creó logs/metricas.csv"
[ "$(wc -l < "$CSV")" -eq 3 ] || fallo "se esperaban encabezado + 2 filas"
grep -q ",ok," "$CSV" || fallo "estado ok no registrado"
if grep -qi "hora\|tres de la tarde" "$CSV"; then fallo "el CSV contiene texto de la conversación"; fi

# --- Caso 2: falla en SLM → ERROR con etapa, código y siguiente paso ----------
escribir_config falla
set +e
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 1 --auto --wav "$ENTRADA" 2>&1)"
codigo=$?
set -e
echo "$SALIDA"
[ "$codigo" -ne 0 ] || fallo "un turno con error debe devolver código distinto de 0"
grep -qF "[ERROR] Etapa: SLM | Código de salida: 42" <<<"$SALIDA" || fallo "diagnóstico incompleto"
grep -qF "Siguiente paso:" <<<"$SALIDA" || fallo "falta el siguiente paso"
grep -q ",error,SLM,42," "$CSV" || fallo "la métrica de error no se registró"

# --- Caso 3: audio demasiado corto → ERROR en GRABANDO ------------------------
CORTO="$TRABAJO/corto.wav"
ffmpeg -hide_banner -loglevel error -nostdin -y -f lavfi -i "sine=frequency=440:duration=2" "$CORTO"
escribir_config llama-completion
set +e
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 1 --auto --wav "$CORTO" 2>&1)"
set -e
grep -qF "[ERROR] Etapa: GRABANDO" <<<"$SALIDA" || fallo "no se rechazó el audio corto"

# --- Caso 4: respuesta rápida → STT pasa directo a TTS sin cargar el SLM -----
# El "SLM" configurado es el binario que falla: si se ejecutara, el turno fallaría.
escribir_config falla
export FALSO_TRANSCRIPCION="¡Hola! ¿Cómo estás?" FALSO_TTS_ESPERADO="Muy bien, gracias."
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 1 --auto --wav "$ENTRADA" 2>&1)" \
    || { echo "$SALIDA"; fallo "la respuesta rápida no debió cargar el SLM"; }
echo "$SALIDA"
grep -qF "respuesta rápida: saludo" <<<"$SALIDA" || fallo "no se usó la respuesta rápida"
if grep -qF "[SLM]" <<<"$SALIDA"; then fallo "el turno pasó por el SLM"; fi
[ "$(tail -n 1 "$CSV" | awk -F, '{print $NF}')" = "atajo" ] || fallo "la métrica no registró ruta=atajo"
if grep -qi "hola\|muy bien" "$CSV"; then fallo "el CSV contiene texto de la conversación"; fi

# --- Caso 5: frase con contenido → sí va al SLM (aquí falla a propósito) ------
set +e
export FALSO_TRANSCRIPCION="Hola, explícame la fotosíntesis"
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 1 --auto --wav "$ENTRADA" 2>&1)"
set -e
grep -qF "[ERROR] Etapa: SLM" <<<"$SALIDA" || fallo "una pregunta real debe ir al SLM"

# --- Caso 6: tema bloqueado → respuesta fija, sin SLM -------------------------
export FALSO_TRANSCRIPCION="Jarvis guarda mi contraseña del correo para mañana por favor"
export FALSO_TTS_ESPERADO="No manejo contraseñas."
SALIDA="$("$JARVIS" --config "$TRABAJO/config/config.json" --turnos 1 --auto --wav "$ENTRADA" 2>&1)" \
    || { echo "$SALIDA"; fallo "el tema bloqueado no debió cargar el SLM"; }
[ "$(tail -n 1 "$CSV" | awk -F, '{print $NF}')" = "bloqueo" ] || fallo "la métrica no registró ruta=bloqueo"

# --- Caso 7: "adiós" termina una sesión sin límite de turnos -------------------
export FALSO_TRANSCRIPCION="Adiós" FALSO_TTS_ESPERADO="Hasta luego."
SALIDA="$(timeout 30 "$JARVIS" --config "$TRABAJO/config/config.json" --turnos 0 --auto --wav "$ENTRADA" 2>&1)" \
    || { echo "$SALIDA"; fallo "la despedida no terminó la sesión"; }
unset FALSO_TRANSCRIPCION FALSO_TTS_ESPERADO
grep -qF "Sesión terminada por el usuario." <<<"$SALIDA" || fallo "falta el mensaje de fin de sesión"
grep -qF "Turnos exitosos: 1 " <<<"$SALIDA" || fallo "la sesión debió terminar tras 1 turno"

# --- Temporales borrados -----------------------------------------------------
if [ -n "$(ls -A "$TMPDIR")" ]; then
    ls -la "$TMPDIR"
    fallo "quedaron archivos temporales"
fi

echo "PRUEBA DE INTEGRACIÓN: OK"
