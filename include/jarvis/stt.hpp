// Etapa STT: whisper.cpp (whisper-cli) con modelo multilingüe y -l es.
#pragma once

#include <string>
#include <vector>

#include "jarvis/config.hpp"
#include "jarvis/estados.hpp"
#include "jarvis/util.hpp"

namespace jarvis {

struct ResultadoStt {
    bool ok = false;
    FalloEtapa fallo;
    std::string texto;
    long ms = 0;
    long rss_kb = 0;
    double rtf = 0.0;  // tiempo de proceso / duración del audio
};

// Construye el argv exacto de whisper-cli (expuesto para pruebas).
std::vector<std::string> argv_whisper(const Config& c, const std::string& wav,
                                      const std::string& base_salida);

ResultadoStt transcribir(const Config& c, const DirTemporal& dir, const std::string& wav,
                         double duracion_s);

}  // namespace jarvis
