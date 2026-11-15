// Etapa TTS: voz Piper (es_MX) con sherpa-onnx-offline-tts y reproducción.
// Respaldo SOLO de demostración: termux-tts-speak.
#pragma once

#include <string>
#include <vector>

#include "jarvis/config.hpp"
#include "jarvis/estados.hpp"
#include "jarvis/util.hpp"

namespace jarvis {

struct ResultadoTts {
    bool ok = false;
    FalloEtapa fallo;
    long ms_sintesis = 0;
    long ms_reproduccion = 0;
    long rss_kb = 0;
    std::string motor;  // "sherpa" o "termux"
};

// argv exacto de sherpa-onnx-offline-tts. El texto va como ÚLTIMO argumento
// separado (ya saneado, sin guiones iniciales), nunca interpolado en un shell.
std::vector<std::string> argv_sherpa(const Config& c, const std::string& wav_salida,
                                     const std::string& texto);

ResultadoTts sintetizar_y_reproducir(const Config& c, const DirTemporal& dir,
                                     const std::string& texto);

}  // namespace jarvis
