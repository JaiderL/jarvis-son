#!/usr/bin/env bash
# Diagnóstico de Termux:API: comunicación, micrófono y reproducción.
# Todas las llamadas termux-* tienen tiempo límite para no quedarse colgadas.
#
# Uso: bash scripts/probar_api.sh
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"

requerir_comando termux-battery-status "pkg install termux-api (y la APP Termux:API desde la misma fuente que Termux)"
requerir_comando termux-microphone-record "pkg install termux-api"
requerir_comando ffmpeg
requerir_comando timeout coreutils
crear_tmp

# --- 1. ¿Responde la app Termux:API? -----------------------------------------
info "1/4 Probando comunicación con la app Termux:API (termux-battery-status, máx. 10 s)"
set +e
SALIDA_BATERIA="$(con_timeout 10 termux-battery-status 2>&1)"
codigo=$?
set -e
if [ "$codigo" -eq 124 ] || [ "$codigo" -eq 137 ]; then
    error "termux-battery-status se quedó colgado (sin respuesta en 10 s)."
    cat >&2 <<'EOF'
        Causas probables:
          1. La APP Termux:API no está instalada (el paquete 'termux-api' solo trae los comandos).
          2. Termux y Termux:API vienen de fuentes distintas (una de F-Droid y otra de
             Google Play/GitHub). Deben ser de la MISMA fuente porque se firman con la
             misma llave. Desinstale ambas e instálelas desde F-Droid.
          3. La app Termux:API nunca se abrió o Android la tiene restringida: ábrala una vez
             y en Ajustes > Apps > Termux:API > Batería elija "Sin restricciones".
EOF
    exit 1
elif [ "$codigo" -ne 0 ]; then
    morir "termux-battery-status falló (código $codigo): $SALIDA_BATERIA" "pkg install termux-api"
fi
ok "Termux:API responde"
printf '%s\n' "$SALIDA_BATERIA" | jq -c '{porcentaje: .percentage, estado: .status}' 2>/dev/null || true

# --- 2. Grabación de 4 s ------------------------------------------------------
GRAB="$JARVIS_TMP/prueba.m4a"
WAV="$JARVIS_TMP/prueba16k.wav"
info "2/4 Grabando 4 s: hable ahora..."
con_timeout 10 termux-microphone-record -f "$GRAB" -l 4 -e aac -c 1 \
    || morir "No se pudo iniciar la grabación." "Conceda permiso de micrófono a Termux:API (Ajustes > Apps > Termux:API > Permisos)."
sleep 5
con_timeout 10 termux-microphone-record -q >/dev/null 2>&1 || true
sleep 1
if [ ! -s "$GRAB" ]; then
    morir "Grabación vacía: el archivo no existe o pesa 0 bytes." \
          "Conceda el permiso de micrófono a Termux:API y cierre otras apps que usen el micrófono."
fi
ok "Grabación creada ($(stat -c %s "$GRAB") bytes)"

# --- 3. Conversión a WAV s16le mono 16 kHz -----------------------------------
info "3/4 Convirtiendo a WAV PCM s16le, mono, 16 kHz"
con_timeout 30 ffmpeg -hide_banner -loglevel error -nostdin -y -i "$GRAB" \
    -ac 1 -ar 16000 -c:a pcm_s16le "$WAV" || morir "ffmpeg no pudo convertir la grabación." "pkg install ffmpeg"
if command -v ffprobe >/dev/null 2>&1; then
    ffprobe -v error -show_entries stream=codec_name,sample_rate,channels -show_entries format=duration \
        -of default=noprint_wrappers=1 "$WAV"
fi
VOLUMEN="$(ffmpeg -hide_banner -nostdin -i "$WAV" -af volumedetect -f null - 2>&1 | grep -o 'mean_volume: [-0-9.]* dB' || true)"
info "Nivel de audio: ${VOLUMEN:-desconocido} (si es menor a -60 dB, la grabación está en silencio)"

# --- 4. Reproducción ----------------------------------------------------------
info "4/4 Reproduciendo la grabación (suba el volumen multimedia)"
if command -v play-audio >/dev/null 2>&1; then
    con_timeout 15 play-audio "$WAV" || aviso "play-audio falló; pruebe: pkg install play-audio"
else
    aviso "play-audio no está instalado; se usa termux-media-player"
    con_timeout 10 termux-media-player play "$WAV" || aviso "termux-media-player falló"
    sleep 5
    con_timeout 10 termux-media-player stop >/dev/null 2>&1 || true
fi

echo
ok "Termux:API funciona. Los archivos de prueba se borran al salir."
echo "Siguiente paso: bash scripts/probar_stt.sh"
