#!/usr/bin/env bash
# Prueba la etapa TTS (voz Piper es_MX con sherpa-onnx) y la reproducción.
#
# Uso:
#   bash scripts/probar_tts.sh [base|alto] ["texto"]
#   bash scripts/probar_tts.sh --respaldo ["texto"]   prueba termux-tts-speak (solo demostración)
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"

TEXTO_DEF="Hola, soy Jarvis-son. Esta es una prueba de voz en español."

if [ "${1:-}" = "--respaldo" ]; then
    requerir_comando termux-tts-speak "pkg install termux-api"
    TEXTO="${2:-$TEXTO_DEF}"
    aviso "Respaldo SOLO de demostración: usa el motor de voz de Android, no Piper."
    # El texto se entrega por stdin, no se interpola en un comando.
    printf '%s' "$TEXTO" | con_timeout 30 termux-tts-speak -l es \
        || morir "termux-tts-speak falló o se colgó." "bash scripts/probar_api.sh"
    ok "Respaldo termux-tts-speak funciona."
    exit 0
fi

requerir_orquestador
requerir_comando jq
PERFIL="$(perfil_activo)"
if [ "${1:-}" = "base" ] || [ "${1:-}" = "alto" ]; then PERFIL="$1"; shift; fi
validar_perfil "$PERFIL"
TEXTO="${1:-$TEXTO_DEF}"
cd "$RAIZ"

command -v play-audio >/dev/null 2>&1 || aviso "play-audio no está instalado: pkg install play-audio"
info "Perfil $PERFIL — sintetizando y reproduciendo (suba el volumen multimedia)"
"$JARVIS_BIN" --perfil "$PERFIL" --prueba tts --texto "$TEXTO" \
    || morir "La etapa TTS falló." "Revise el diagnóstico de arriba o pruebe: bash scripts/probar_tts.sh --respaldo"
ok "TTS funciona. Siguiente paso: ./build/jarvis (orquestador completo)"
