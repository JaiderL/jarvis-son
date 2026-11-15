#!/usr/bin/env bash
# Prueba la etapa SLM (llama.cpp, una sola ejecución, thinking desactivado).
#
# Uso: bash scripts/probar_slm.sh [base|alto] ["pregunta"]
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"
requerir_orquestador
requerir_comando jq

PERFIL="$(perfil_activo)"
if [ "${1:-}" = "base" ] || [ "${1:-}" = "alto" ]; then PERFIL="$1"; shift; fi
validar_perfil "$PERFIL"
PREGUNTA="${1:-¿Cuál es la capital de Colombia?}"
cd "$RAIZ"

info "Perfil $PERFIL — pregunta: $PREGUNTA"
info "Se carga el modelo, responde una vez y se libera la memoria."
# La pregunta va como un argumento separado; el orquestador arma el prompt
# ChatML en un archivo temporal y llama.cpp lo lee con -f (nunca pasa por un shell).
set +e
salida="$("$JARVIS_BIN" --perfil "$PERFIL" --prueba slm --texto "$PREGUNTA" 2>&1)"
codigo=$?
set -e
printf '%s\n' "$salida"
[ "$codigo" -eq 0 ] || morir "La etapa SLM falló (código $codigo)." "Revise el diagnóstico de arriba."

if printf '%s' "$salida" | grep -qi '<think>'; then
    aviso "La respuesta contiene <think>: el modo thinking no se desactivó. Revise construir_prompt_chatml() en src/slm.cpp."
fi
ok "SLM funciona. Siguiente paso: bash scripts/probar_tts.sh"
