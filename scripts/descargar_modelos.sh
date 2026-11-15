#!/usr/bin/env bash
# Descarga los modelos de un perfil desde models/manifest.json y verifica SHA-256.
# Idempotente: si el archivo ya existe y su SHA-256 coincide, no lo descarga.
#
# Uso: bash scripts/descargar_modelos.sh [base|alto]
set -Eeuo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/comun.sh"

PERFIL="${1:-base}"
validar_perfil "$PERFIL"
MANIFIESTO="$RAIZ/models/manifest.json"
requerir_comando jq
requerir_comando curl
requerir_comando sha256sum coreutils
[ -f "$MANIFIESTO" ] || morir "No existe $MANIFIESTO"

DESCARGAS="$RAIZ/models/.descargas"
mkdir -p "$DESCARGAS"

pendientes=0

# verificar_sha ARCHIVO ESPERADO ID → 0 si coincide o es PENDIENTE (con aviso)
verificar_sha() {
    local archivo="$1" esperado="$2" id="$3" real
    info "Calculando SHA-256 de $(basename "$archivo")..."
    real="$(sha256sum "$archivo" | cut -d' ' -f1)"
    if [ "$esperado" = "PENDIENTE" ]; then
        aviso "$id no tiene SHA-256 registrado en el manifiesto."
        aviso "SHA-256 calculado: $real"
        aviso "Verifíquelo contra la fuente oficial y regístrelo en models/manifest.json."
        pendientes=$((pendientes + 1))
        return 0
    fi
    if [ "$real" != "$esperado" ]; then
        error "SHA-256 NO coincide para $id"
        error "  esperado: $esperado"
        error "  obtenido: $real"
        return 1
    fi
    ok "SHA-256 correcto: $id"
}

# Una línea por modelo, separada por tabuladores (sin eval).
mapfile -t MODELOS < <(jq -r --arg p "$PERFIL" \
    '.modelos[] | select(.perfiles | index($p)) | [.id, .archivo, .destino, .formato, .url, .sha256, (.tamano_bytes|tostring)] | @tsv' \
    "$MANIFIESTO")
[ "${#MODELOS[@]}" -gt 0 ] || morir "El perfil '$PERFIL' no tiene modelos en el manifiesto."

info "Perfil '$PERFIL': ${#MODELOS[@]} modelos"
for linea in "${MODELOS[@]}"; do
    IFS=$'\t' read -r id archivo destino formato url sha tamano <<<"$linea"
    # Rechaza rutas sospechosas en el manifiesto.
    case "$destino" in models/*) ;; *) morir "Destino inválido en el manifiesto: $destino" ;; esac
    case "$destino$archivo" in *..*) morir "Ruta inválida en el manifiesto: $destino" ;; esac
    case "$url" in https://*) ;; *) morir "Solo se permiten URLs https: $url" ;; esac

    descarga="$DESCARGAS/$archivo"
    [ "$formato" = "archivo" ] && descarga="$RAIZ/$destino"

    echo
    info "$id → $destino ($((tamano / 1024 / 1024)) MB)"

    if [ "$formato" = "archivo" ] && [ -f "$descarga" ] && [ "$(stat -c %s "$descarga")" = "$tamano" ]; then
        if verificar_sha "$descarga" "$sha" "$id"; then
            ok "Ya estaba descargado: $destino"
            continue
        fi
        aviso "El archivo existente está dañado; se descargará de nuevo."
        rm -f "$descarga"
    fi
    if [ "$formato" = "tar.bz2" ] && [ -d "$RAIZ/$destino" ] && [ -f "$RAIZ/$destino/tokens.txt" ]; then
        ok "Voz ya instalada: $destino"
        continue
    fi

    # Descarga reanudable a .parte y luego se renombra.
    if ! curl -L -# --fail --retry 3 --retry-delay 5 --connect-timeout 30 -C - \
            -o "$descarga.parte" "$url"; then
        morir "No se pudo descargar $url" "Revise la conexión y vuelva a ejecutar (la descarga se reanuda)."
    fi
    mv -f "$descarga.parte" "$descarga"
    real_tam="$(stat -c %s "$descarga")"
    if [ "$real_tam" != "$tamano" ]; then
        aviso "Tamaño distinto al del manifiesto ($real_tam vs $tamano bytes)."
    fi
    if ! verificar_sha "$descarga" "$sha" "$id"; then
        rm -f "$descarga"
        morir "Se borró la descarga de $id por SHA-256 incorrecto." "Vuelva a ejecutar el script."
    fi

    if [ "$formato" = "tar.bz2" ]; then
        info "Extrayendo $archivo"
        tar -xjf "$descarga" -C "$RAIZ/models" || morir "No se pudo extraer $archivo" "pkg install tar bzip2"
        [ -f "$RAIZ/$destino/tokens.txt" ] || morir "La voz extraída no tiene tokens.txt en $destino"
        rm -f "$descarga"
        ok "Voz instalada en $destino"
    fi
done

echo
if [ "$pendientes" -gt 0 ]; then
    aviso "$pendientes archivo(s) sin SHA-256 registrado: anote los valores mostrados en models/manifest.json."
fi
ok "Modelos del perfil '$PERFIL' listos. Ya puede activar el modo avión."
du -sh "$RAIZ/models" 2>/dev/null || true
echo "Siguiente paso: bash scripts/probar_api.sh"
