#include "jarvis/metricas.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "jarvis/util.hpp"

namespace fs = std::filesystem;

namespace jarvis {

namespace {

std::string decimal(double v, int decimales) {
    char bufer[64];
    std::snprintf(bufer, sizeof bufer, "%.*f", decimales, v);
    return bufer;
}

// Los campos de texto son identificadores internos; aun así se filtran
// comas, comillas y saltos de línea para no romper el CSV.
std::string campo(const std::string& s) {
    std::string r;
    for (char c : s)
        if (c != ',' && c != '"' && c != '\n' && c != '\r') r += c;
    return r;
}

}  // namespace

std::string encabezado_csv() {
    return "fecha,perfil,turno,estado,etapa_error,codigo_error,ms_grabacion,ms_conversion,"
           "ms_stt,ms_slm,ms_tts,ms_reproduccion,ms_total,duracion_audio_s,rtf_stt,"
           "rss_ffmpeg_kb,rss_stt_kb,rss_slm_kb,rss_tts_kb,motor_tts,ruta";
}

std::string fila_csv(const MetricasTurno& m) {
    std::string s;
    s += campo(m.fecha) + ",";
    s += campo(m.perfil) + ",";
    s += std::to_string(m.turno) + ",";
    s += campo(m.estado) + ",";
    s += campo(m.etapa_error) + ",";
    s += std::to_string(m.codigo_error) + ",";
    s += std::to_string(m.ms_grabacion) + ",";
    s += std::to_string(m.ms_conversion) + ",";
    s += std::to_string(m.ms_stt) + ",";
    s += std::to_string(m.ms_slm) + ",";
    s += std::to_string(m.ms_tts) + ",";
    s += std::to_string(m.ms_reproduccion) + ",";
    s += std::to_string(m.ms_total) + ",";
    s += decimal(m.duracion_audio_s, 2) + ",";
    s += decimal(m.rtf_stt, 3) + ",";
    s += std::to_string(m.rss_ffmpeg_kb) + ",";
    s += std::to_string(m.rss_stt_kb) + ",";
    s += std::to_string(m.rss_slm_kb) + ",";
    s += std::to_string(m.rss_tts_kb) + ",";
    s += campo(m.motor_tts) + ",";
    s += campo(m.ruta);
    return s;
}

bool anexar_metricas(const std::string& ruta, const MetricasTurno& m) {
    std::error_code ec;
    fs::path p(ruta);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    // Si el archivo existe con otro encabezado (versión anterior del CSV), se
    // conserva aparte con otro nombre y se empieza uno nuevo.
    if (tamano_archivo(ruta) > 0) {
        std::ifstream viejo(ruta);
        std::string primera;
        std::getline(viejo, primera);
        if (primera != encabezado_csv()) {
            viejo.close();
            std::string destino = ruta + ".anterior";
            for (int i = 1; existe_archivo(destino); ++i)
                destino = ruta + ".anterior" + std::to_string(i);
            fs::rename(ruta, destino, ec);
        }
    }
    const bool nuevo = tamano_archivo(ruta) <= 0;
    std::ofstream f(ruta, std::ios::app);
    if (!f) return false;
    if (nuevo) f << encabezado_csv() << "\n";
    f << fila_csv(m) << "\n";
    return static_cast<bool>(f);
}

}  // namespace jarvis
