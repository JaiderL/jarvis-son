#!/usr/bin/env bash
# Prueba la etapa STT (whisper.cpp, -l es) por separado.
#
# Uso:
#   bash scripts/probar_stt.sh [base|alto]                 graba del micrófono y transcribe
#   bash scripts/probar_stt.sh [base|alto] --wav AUDIO      transcribe un archivo existente
#   bash scripts/probar_stt.sh [base|alto] --wer           graba las 20 frases de referencia,
#                                                           guarda los audios y calcula el WER
#   bash scripts/probar_stt.sh [base|alto] --wer-reusar    vuelve a transcribir los audios ya
#                                                           grabados (compara configuraciones)
#
# Los audios de --wer se guardan en logs/wer_audios/ (solo las frases de referencia, nunca
# conversaciones). Bórrelos con: rm -rf logs/wer_audios
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
    --wer|--wer-reusar)
        requerir_comando python3 "pkg install python"
        REFS="$RAIZ/tests/frases_referencia.txt"
        AUDIOS="$RAIZ/logs/wer_audios"
        HIP="$RAIZ/logs/hipotesis_stt_${PERFIL}.txt"  # solo frases de prueba
        mkdir -p "$AUDIOS"
        if [ "$1" = "--wer-reusar" ] && [ ! -f "$AUDIOS/01.wav" ]; then
            morir "No hay audios grabados en $AUDIOS." "Primero ejecute: bash scripts/probar_stt.sh --wer"
        fi
        : > "$HIP"
        mapfile -t FRASES < <(grep -v '^[[:space:]]*$' "$REFS")
        total="${#FRASES[@]}"
        inicio_s=$SECONDS
        for i in "${!FRASES[@]}"; do
            n=$((i + 1))
            wav="$AUDIOS/$(printf '%02d' "$n").wav"
            if [ "$1" = "--wer" ]; then
                echo
                info "Frase $n/$total — cuando aparezca 'Hable ahora', lea en voz alta:"
                printf '    "%s"\n' "${FRASES[$i]}"
                read -r -p "    Presione Enter para grabar..." _ </dev/tty
                args=(--prueba stt --guardar-wav "$wav")
            else
                [ -f "$wav" ] || morir "Falta el audio $wav." "Grabe de nuevo con --wer"
                args=(--prueba stt --wav "$wav")
            fi
            set +e
            salida="$("$JARVIS_BIN" --perfil "$PERFIL" --sin-metricas "${args[@]}" </dev/null 2>&1)"
            set -e
            transcripcion="$(printf '%s\n' "$salida" | sed -n 's/^TRANSCRIPCION: //p')"
            printf '    %02d: %s\n' "$n" "${transcripcion:-<vacía>}"
            printf '%s\n' "$transcripcion" >> "$HIP"
        done
        echo
        info "Perfil $PERFIL | modelo $(jq -r ".perfiles.${PERFIL}.modelo_stt" "$CONFIG") | beam $(jq -r ".perfiles.${PERFIL}.stt_beam" "$CONFIG") | $((SECONDS - inicio_s)) s en total"
        python3 "$RAIZ/tests/calcular_wer.py" "$REFS" "$HIP"
        ;;
    *)
        morir "Opción desconocida: $1" "Use --wav RUTA, --wer o --wer-reusar"
        ;;
esac
