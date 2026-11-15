#!/usr/bin/env bash
# Funciones compartidas por los scripts de Jarvis-son. Se carga con:
#   source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"
# Nunca se usa eval ni se interpola texto del usuario en comandos.

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export RAIZ
JARVIS_BIN="$RAIZ/build/jarvis"
CONFIG="$RAIZ/config/config.json"

if [ -t 1 ]; then
    C_ROJO=$'\033[31m'; C_VERDE=$'\033[32m'; C_AMARILLO=$'\033[33m'; C_AZUL=$'\033[36m'; C_FIN=$'\033[0m'
else
    C_ROJO=""; C_VERDE=""; C_AMARILLO=""; C_AZUL=""; C_FIN=""
fi

info()  { printf '%s[INFO]%s %s\n' "$C_AZUL" "$C_FIN" "$*"; }
ok()    { printf '%s[ OK ]%s %s\n' "$C_VERDE" "$C_FIN" "$*"; }
aviso() { printf '%s[AVISO]%s %s\n' "$C_AMARILLO" "$C_FIN" "$*" >&2; }
error() { printf '%s[ERROR]%s %s\n' "$C_ROJO" "$C_FIN" "$*" >&2; }

# Termina con un mensaje claro y el siguiente paso sugerido.
morir() {
    error "$1"
    [ -n "${2:-}" ] && printf '        Siguiente paso: %s\n' "$2" >&2
    exit 1
}

es_termux() { [ -n "${PREFIX:-}" ] && [[ "$PREFIX" == *com.termux* ]]; }

requerir_comando() {
    command -v "$1" >/dev/null 2>&1 || morir "No se encontró el comando '$1'." "${2:-pkg install $1}"
}

# Ejecuta un comando con tiempo límite (para que termux-* nunca se cuelgue).
# Uso: con_timeout SEGUNDOS comando args...
con_timeout() {
    local segundos="$1"; shift
    timeout --kill-after=3 "$segundos" "$@"
}

# Directorio temporal propio que se borra al salir (éxito o error).
crear_tmp() {
    local base="${TMPDIR:-/tmp}"
    JARVIS_TMP="$(mktemp -d "$base/jarvis-script-XXXXXX")" || morir "No se pudo crear un temporal en $base"
    trap 'rm -rf "$JARVIS_TMP"' EXIT
}

requerir_orquestador() {
    [ -x "$JARVIS_BIN" ] || morir "No existe $JARVIS_BIN." "bash scripts/instalar.sh"
}

# Lee el perfil activo de config.json (requiere jq).
perfil_activo() {
    jq -r '.perfil_activo' "$CONFIG"
}

validar_perfil() {
    case "$1" in
        base|alto) ;;
        *) morir "Perfil inválido: '$1'. Use 'base' o 'alto'." ;;
    esac
}
