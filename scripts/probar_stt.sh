#!/usr/bin/env bash
# Prueba la etapa STT (whisper.cpp, -l es) por separado.
#
# Uso:
#   bash scripts/probar_stt.sh [base|alto]                 graba del micrófono y transcribe
#   bash scripts/probar_stt.sh [base|alto] --wav AUDIO      transcribe un archivo existente
#   bash scripts/probar_stt.sh [base|alto] --wer           graba las 20 frases de referencia
#                                                           y calcula el WER (meta ≥ 18/20)
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"
requerir_orquestador
requerir_comando jq

PERFIL="$(perfil_activo)"
if [ "${1:-}" = "base" ] || [ "${1:-}" = "alto" ]; then PERFIL="$1"; shift; fi
validar_perfil "$PERFIL"
cd "$RAIZ"

case "${1:-}" in
    "")
        info "Perfil $PERFIL: se grabarán unos segundos. Diga una frase en español."
        "$JARVIS_BIN" --perfil "$PERFIL" --prueba stt
        ;;
    --wav)
        [ -f "${2:-}" ] || morir "Indique un archivo de audio existente: --wav RUTA"
        "$JARVIS_BIN" --perfil "$PERFIL" --prueba stt --wav "$2"
        ;;
    --wer)
        requerir_comando python3 "pkg install python"
        REFS="$RAIZ/tests/frases_referencia.txt"
        HIP="$RAIZ/logs/hipotesis_stt.txt"   # solo frases de prueba, no conversación real
        mkdir -p "$RAIZ/logs"
        : > "$HIP"
        n=0
        while IFS= read -r frase || [ -n "$frase" ]; do
            [ -z "$frase" ] && continue
            n=$((n + 1))
            echo
            info "Frase $n/20 — cuando aparezca 'Hable ahora', lea en voz alta:"
            printf '    "%s"\n' "$frase"
            read -r -p "    Presione Enter para grabar..." _ </dev/tty
            set +e
            salida="$("$JARVIS_BIN" --perfil "$PERFIL" --prueba stt --sin-metricas 2>&1)"
            set -e
            transcripcion="$(printf '%s\n' "$salida" | sed -n 's/^TRANSCRIPCION: //p')"
            printf '    Transcripción: %s\n' "${transcripcion:-<vacía>}"
            printf '%s\n' "$transcripcion" >> "$HIP"
        done < "$REFS"
        echo
        python3 "$RAIZ/tests/calcular_wer.py" "$REFS" "$HIP"
        ;;
    *)
        morir "Opción desconocida: $1" "Use --wav RUTA o --wer"
        ;;
esac
