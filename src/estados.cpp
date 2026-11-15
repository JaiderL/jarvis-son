#include "jarvis/estados.hpp"

namespace jarvis {

const char* nombre_estado(Estado e) {
    switch (e) {
        case Estado::Listo: return "LISTO";
        case Estado::Grabando: return "GRABANDO";
        case Estado::Stt: return "STT";
        case Estado::Slm: return "SLM";
        case Estado::Tts: return "TTS";
        case Estado::Error: return "ERROR";
    }
    return "DESCONOCIDO";
}

bool transicion_valida(Estado desde, Estado hacia) {
    if (hacia == Estado::Error) return desde != Estado::Error && desde != Estado::Listo;
    switch (desde) {
        case Estado::Listo: return hacia == Estado::Grabando;
        case Estado::Grabando: return hacia == Estado::Stt;
        case Estado::Stt: return hacia == Estado::Slm;
        case Estado::Slm: return hacia == Estado::Tts;
        case Estado::Tts: return hacia == Estado::Listo;
        case Estado::Error: return hacia == Estado::Listo;
    }
    return false;
}

std::string formatear_fallo(const FalloEtapa& f) {
    std::string s = "[ERROR] Etapa: ";
    s += nombre_estado(f.etapa);
    s += " | Código de salida: " + std::to_string(f.codigo);
    s += "\n  Detalle: " + f.detalle;
    s += "\n  Siguiente paso: " + f.siguiente_paso;
    return s;
}

}  // namespace jarvis
