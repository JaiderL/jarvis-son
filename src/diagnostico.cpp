#include "jarvis/diagnostico.hpp"

#include <cerrno>

#include "jarvis/util.hpp"

namespace jarvis {

FalloEtapa fallo_de_proceso(Estado etapa, const ResultadoProceso& r, const std::string& binario,
                            const std::string& paso_general) {
    FalloEtapa f;
    f.etapa = etapa;
    f.codigo = r.codigo_reporte();
    f.detalle = binario + ": " + r.describir();
    if (!r.lanzado) {
        f.siguiente_paso = (r.errno_lanzamiento == ENOENT)
                               ? "No se encontró el binario. Ejecute: bash scripts/instalar.sh"
                               : "Revise permisos de ejecución (chmod +x) del binario.";
        return f;
    }
    if (r.interrumpido) {
        f.siguiente_paso = "Turno cancelado con Ctrl+C. Vuelva a intentarlo.";
        return f;
    }
    if (r.timeout) {
        f.siguiente_paso =
            "La etapa tardó demasiado. Cierre otras apps, use el perfil \"base\" o aumente "
            "tiempos_limite_s en config/config.json.";
        return f;
    }
    if (r.senal == 9) {
        f.siguiente_paso =
            "Android mató el proceso (probable falta de RAM / OOM). Cierre apps en segundo "
            "plano o use un modelo más pequeño.";
        return f;
    }
    f.siguiente_paso = paso_general;
    std::string cola = ultimas_lineas(r.error, 4);
    if (!cola.empty()) f.detalle += "\n  stderr (últimas líneas):\n" + cola;
    return f;
}

}  // namespace jarvis
