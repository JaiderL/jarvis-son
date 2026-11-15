// Lectura y validación de archivos WAV (PCM s16le).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jarvis {

struct InfoWav {
    int formato = 0;  // 1 = PCM entero
    int canales = 0;
    int frecuencia = 0;  // Hz
    int bits = 0;
    std::uint32_t bytes_datos = 0;
    double duracion_s = 0.0;
    double rms = 0.0;  // energía media (solo para PCM de 16 bits)
};

// Lee la cabecera y calcula duración y RMS. Devuelve false y llena error si falla.
bool leer_info_wav(const std::string& ruta, InfoWav& info, std::string& error);

// Valida el formato exigido para whisper: PCM s16le, mono, 16 kHz, duración
// dentro de [min_s, max_s] y energía mínima (descarta grabaciones vacías).
// Devuelve vacío si es válido o el motivo del rechazo.
std::string validar_wav_stt(const InfoWav& info, double min_s, double max_s, double rms_minimo);

// Escribe un WAV PCM s16le (se usa en las pruebas).
bool escribir_wav_s16(const std::string& ruta, const std::vector<std::int16_t>& muestras,
                      int frecuencia, int canales);

}  // namespace jarvis
