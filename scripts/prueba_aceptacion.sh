#!/usr/bin/env bash
# Prueba de aceptación: 10 turnos seguidos MIC → STT → SLM → TTS y reporte
# de p50/p95 por etapa y memoria máxima por proceso hijo.
#
# Uso: bash scripts/prueba_aceptacion.sh [base|alto] [turnos]
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"
requerir_orquestador
requerir_comando jq
requerir_comando python3 "pkg install python"

PERFIL="$(perfil_activo)"
if [ "${1:-}" = "base" ] || [ "${1:-}" = "alto" ]; then PERFIL="$1"; shift; fi
validar_perfil "$PERFIL"
TURNOS="${1:-10}"
[[ "$TURNOS" =~ ^[0-9]+$ ]] && [ "$TURNOS" -ge 1 ] || morir "El número de turnos debe ser un entero positivo."
cd "$RAIZ"

CSV="$RAIZ/$(jq -r '.metricas.archivo' "$CONFIG")"
FILAS_ANTES=0
[ -f "$CSV" ] && FILAS_ANTES="$(($(wc -l < "$CSV") - 1))"
[ "$FILAS_ANTES" -lt 0 ] && FILAS_ANTES=0

command -v termux-wake-lock >/dev/null 2>&1 && con_timeout 10 termux-wake-lock || true

info "Perfil $PERFIL: $TURNOS turnos seguidos. En cada turno hable cuando aparezca 'Hable ahora'."
info "Sugerencia: use preguntas cortas de tests/frases_referencia.txt."
set +e
"$JARVIS_BIN" --perfil "$PERFIL" --turnos "$TURNOS" --auto
codigo=$?
set -e

echo
info "Reporte de los últimos $TURNOS turnos"
python3 "$RAIZ/tests/reporte_latencias.py" "$CSV" --ultimos "$TURNOS"

if [ "$codigo" -ne 0 ]; then
    aviso "Hubo turnos con error (ver diagnóstico arriba y la columna etapa_error del CSV)."
    exit 1
fi
ok "Prueba de aceptación completa sin errores."
