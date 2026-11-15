// Etapa GRABANDO: termux-microphone-record + FFmpeg → WAV s16le mono 16 kHz.
#pragma once

#include <string>

#include "jarvis/config.hpp"
#include "jarvis/estados.hpp"
#include "jarvis/util.hpp"

namespace jarvis {

struct ResultadoCaptura {
    bool ok = false;
    FalloEtapa fallo;
    std::string wav;  // WAV validado dentro del directorio temporal
    double duracion_s = 0.0;
    double rms = 0.0;
    long ms_grabacion = 0;
    long ms_conversion = 0;
    long rss_ffmpeg_kb = 0;
};

// Si wav_entrada no está vacío, se usa ese archivo en lugar del micrófono
// (útil para pruebas); igual se normaliza con FFmpeg y se valida.
ResultadoCaptura capturar_audio(const Config& c, const DirTemporal& dir,
                                const std::string& wav_entrada);

}  // namespace jarvis
