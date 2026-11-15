// Métricas por turno en logs/metricas.csv.
// PRIVACIDAD: solo números y estados; nunca audio ni texto de la conversación.
#pragma once

#include <string>

namespace jarvis {

struct MetricasTurno {
    std::string fecha;
    std::string perfil;
    int turno = 0;
    std::string estado = "ok";  // "ok" o "error"
    std::string etapa_error;    // vacío si no hubo error
    int codigo_error = 0;
    long ms_grabacion = 0;
    long ms_conversion = 0;
    long ms_stt = 0;
    long ms_slm = 0;
    long ms_tts = 0;  // síntesis
    long ms_reproduccion = 0;
    long ms_total = 0;
    double duracion_audio_s = 0.0;
    double rtf_stt = 0.0;  // tiempo STT / duración del audio
    long rss_ffmpeg_kb = 0;
    long rss_stt_kb = 0;
    long rss_slm_kb = 0;
    long rss_tts_kb = 0;
    std::string motor_tts;  // "sherpa" o "termux"
};

std::string encabezado_csv();
std::string fila_csv(const MetricasTurno& m);

// Agrega la fila al CSV; crea el directorio y el encabezado si no existen.
bool anexar_metricas(const std::string& ruta, const MetricasTurno& m);

}  // namespace jarvis
