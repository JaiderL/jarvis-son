#!/usr/bin/env bash
# Instalación idempotente de Jarvis-son en Termux (Android arm64, sin root).
# Se puede ejecutar varias veces: lo que ya está compilado se omite.
#
# Uso: bash scripts/instalar.sh [--forzar]
#   --forzar   recompila whisper.cpp, llama.cpp, sherpa-onnx y el orquestador.
#
# Variables opcionales:
#   JOBS=2                 hilos de compilación (2 por defecto: evita OOM en 3 GB)
#   GGML_NATIVE=OFF        desactiva -mcpu=native si aparece "Illegal instruction"
#   PERMITIR_NO_TERMUX=1   permite ejecutar en Linux (solo desarrollo)
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"

trap 'error "Falló la línea $LINENO: $BASH_COMMAND"; echo "        Revise el mensaje anterior y la tabla de problemas en docs/INSTALACION.md" >&2' ERR

# Versiones fijadas (verificadas el 2026-11-15). Cambiarlas solo tras probar.
WHISPER_REF="${WHISPER_REF:-v1.9.5}"
LLAMA_REF="${LLAMA_REF:-v1.0.0}"
SHERPA_REF="${SHERPA_REF:-v1.13.8}"  # v2.x quitó espeak-ng: las voces Piper no funcionan en el CLI
JOBS="${JOBS:-2}"
GGML_NATIVE="${GGML_NATIVE:-ON}"  # OFF si aparece "Illegal instruction"
FORZAR=0
[ "${1:-}" = "--forzar" ] && FORZAR=1

TERCEROS="$RAIZ/third_party"
mkdir -p "$TERCEROS" "$RAIZ/models" "$RAIZ/logs"

# ---------------------------------------------------------------------------
info "1/7 Verificando el entorno"
ARQ="$(uname -m)"
if [ "$ARQ" != "aarch64" ] && [ "$ARQ" != "arm64" ]; then
    if [ "${PERMITIR_NO_TERMUX:-0}" = "1" ]; then
        aviso "Arquitectura $ARQ (no arm64). Continuando por PERMITIR_NO_TERMUX=1."
    else
        morir "Arquitectura '$ARQ' no soportada: se requiere arm64 (aarch64)." \
              "Use un teléfono ARM64 con Android 13+ o exporte PERMITIR_NO_TERMUX=1 para desarrollo."
    fi
fi
if es_termux; then
    ok "Termux detectado ($PREFIX), arquitectura $ARQ"
elif [ "${PERMITIR_NO_TERMUX:-0}" != "1" ]; then
    morir "Este script está pensado para Termux." "Instale Termux desde F-Droid (ver docs/INSTALACION.md)."
fi

# ---------------------------------------------------------------------------
info "2/7 Instalando paquetes del sistema"
if es_termux; then
    pkg update -y || morir "pkg update falló." "Ejecute 'termux-change-repo', elija otro espejo y reintente."
    pkg install -y clang cmake make git ffmpeg termux-api jq python curl coreutils play-audio \
        || morir "No se pudieron instalar los paquetes." "Revise la conexión y ejecute 'pkg update'."
    # onnxruntime de Termux (opcional): si no existe, sherpa-onnx descarga el suyo para Android.
    pkg install -y onnxruntime >/dev/null 2>&1 && ok "onnxruntime de Termux instalado" \
        || aviso "onnxruntime no disponible en pkg; sherpa-onnx usará su versión precompilada."
else
    for c in cmake git ffmpeg jq curl; do requerir_comando "$c" "instálelo con su gestor de paquetes"; done
fi
requerir_comando clang++ "pkg install clang"
ok "Compilador: $(clang++ --version | head -n1)"

# ---------------------------------------------------------------------------
# clonar_o_actualizar URL DIRECTORIO REF
clonar_o_actualizar() {
    local url="$1" dir="$2" ref="$3"
    if [ -d "$dir/.git" ]; then
        local actual
        actual="$(git -C "$dir" describe --tags --exact-match 2>/dev/null || echo '?')"
        if [ "$actual" = "$ref" ]; then
            ok "$(basename "$dir") ya está en $ref"
            return 0
        fi
        info "Actualizando $(basename "$dir") de $actual a $ref"
        git -C "$dir" fetch --depth 1 origin tag "$ref" || morir "No se pudo descargar $ref de $url"
        git -C "$dir" checkout -q "$ref"
        rm -rf "$dir/build"
    else
        info "Clonando $url ($ref)"
        git clone --depth 1 --branch "$ref" "$url" "$dir" || morir "No se pudo clonar $url" "Revise la conexión a internet."
    fi
}

# compilar NOMBRE DIRECTORIO BINARIO_ESPERADO OBJETIVO [opciones cmake...]
compilar() {
    local nombre="$1" dir="$2" binario="$3" objetivo="$4"; shift 4
    if [ "$FORZAR" = "0" ] && [ -x "$binario" ]; then
        ok "$nombre ya compilado: $binario"
        return 0
    fi
    info "Compilando $nombre con -j$JOBS (puede tardar 10–40 min; mantenga la pantalla encendida)"
    cmake -S "$dir" -B "$dir/build" -DCMAKE_BUILD_TYPE=Release "$@" \
        || morir "Falló la configuración de $nombre." "Revise que cmake y clang estén instalados."
    if ! cmake --build "$dir/build" --config Release -j "$JOBS" --target "$objetivo"; then
        morir "Falló la compilación de $nombre (¿Android cerró el proceso por memoria?)." \
              "Cierre otras apps, active termux-wake-lock y reintente con: JOBS=1 bash scripts/instalar.sh"
    fi
    [ -x "$binario" ] || morir "La compilación de $nombre terminó pero no existe $binario."
    ok "$nombre compilado: $binario"
}

# ---------------------------------------------------------------------------
info "3/7 whisper.cpp ($WHISPER_REF)"
clonar_o_actualizar https://github.com/ggml-org/whisper.cpp "$TERCEROS/whisper.cpp" "$WHISPER_REF"
compilar "whisper.cpp" "$TERCEROS/whisper.cpp" "$TERCEROS/whisper.cpp/build/bin/whisper-cli" whisper-cli \
    -DGGML_NO_OPENMP=ON -DGGML_NATIVE="$GGML_NATIVE" -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF -DWHISPER_SDL2=OFF

# ---------------------------------------------------------------------------
info "4/7 llama.cpp ($LLAMA_REF)"
clonar_o_actualizar https://github.com/ggml-org/llama.cpp "$TERCEROS/llama.cpp" "$LLAMA_REF"
compilar "llama.cpp" "$TERCEROS/llama.cpp" "$TERCEROS/llama.cpp/build/bin/llama-completion" llama-completion \
    -DGGML_NO_OPENMP=ON -DGGML_NATIVE="$GGML_NATIVE" -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_CURL=OFF

# ---------------------------------------------------------------------------
info "5/7 sherpa-onnx ($SHERPA_REF)"
clonar_o_actualizar https://github.com/k2-fsa/sherpa-onnx "$TERCEROS/sherpa-onnx" "$SHERPA_REF"
OPC_SHERPA=(
    -DBUILD_SHARED_LIBS=ON
    -DSHERPA_ONNX_ENABLE_PYTHON=OFF
    -DSHERPA_ONNX_ENABLE_TESTS=OFF
    -DSHERPA_ONNX_ENABLE_CHECK=OFF
    -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF
    -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF
    -DSHERPA_ONNX_ENABLE_C_API=OFF
    -DSHERPA_ONNX_ENABLE_BINARY=ON
)
if es_termux; then
    CABECERA_ORT="$(find "$PREFIX/include" -name onnxruntime_cxx_api.h 2>/dev/null | head -n1 || true)"
    if [ -n "$CABECERA_ORT" ] && [ -f "$PREFIX/lib/libonnxruntime.so" ]; then
        SHERPA_ONNXRUNTIME_INCLUDE_DIR="$(dirname "$CABECERA_ORT")"
        export SHERPA_ONNXRUNTIME_INCLUDE_DIR
        export SHERPA_ONNXRUNTIME_LIB_DIR="$PREFIX/lib"
        OPC_SHERPA+=(-DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON)
        info "Usando onnxruntime de Termux: $SHERPA_ONNXRUNTIME_INCLUDE_DIR"
    else
        OPC_SHERPA+=(-DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=OFF)
        info "sherpa-onnx descargará onnxruntime precompilado para Android aarch64"
    fi
fi
compilar "sherpa-onnx" "$TERCEROS/sherpa-onnx" "$TERCEROS/sherpa-onnx/build/bin/sherpa-onnx-offline-tts" \
    sherpa-onnx-offline-tts "${OPC_SHERPA[@]}"

# ---------------------------------------------------------------------------
info "6/7 Orquestador Jarvis-son"
if [ "$FORZAR" = "1" ] || [ ! -x "$RAIZ/build/jarvis" ] || [ -n "$(find "$RAIZ/src" "$RAIZ/include" -newer "$RAIZ/build/jarvis" 2>/dev/null | head -n1)" ]; then
    cmake -S "$RAIZ" -B "$RAIZ/build" -DCMAKE_BUILD_TYPE=Release -DJARVIS_PRUEBAS=ON \
        || morir "Falló la configuración del orquestador."
    cmake --build "$RAIZ/build" -j "$JOBS" || morir "Falló la compilación del orquestador."
fi
ctest --test-dir "$RAIZ/build" --output-on-failure -R unitarias \
    || morir "Fallaron las pruebas unitarias del orquestador." "Reporte la salida en un issue."
ok "Orquestador listo: $RAIZ/build/jarvis"

# ---------------------------------------------------------------------------
info "7/7 Resumen"
"$RAIZ/third_party/whisper.cpp/build/bin/whisper-cli" --help >/dev/null 2>&1 && ok "whisper-cli responde" || aviso "whisper-cli no respondió a --help"
"$RAIZ/third_party/llama.cpp/build/bin/llama-completion" --version 2>&1 | head -n2 || true
"$RAIZ/third_party/sherpa-onnx/build/bin/sherpa-onnx-offline-tts" --help >/dev/null 2>&1 && ok "sherpa-onnx-offline-tts responde" || aviso "sherpa-onnx-offline-tts no respondió a --help"
"$RAIZ/build/jarvis" --version

echo
ok "Instalación completa."
echo "Siguiente paso: bash scripts/descargar_modelos.sh base"
