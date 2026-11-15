// Pruebas de lectura y validación de WAV.
#include <cmath>
#include <cstdint>
#include <vector>

#include "jarvis/util.hpp"
#include "jarvis/wav.hpp"
#include "prueba.hpp"

using namespace jarvis;

namespace {

std::vector<std::int16_t> tono(double segundos, int frecuencia, double amplitud) {
    std::vector<std::int16_t> m(static_cast<std::size_t>(segundos * frecuencia));
    for (std::size_t i = 0; i < m.size(); ++i)
        m[i] = static_cast<std::int16_t>(
            amplitud * std::sin(2.0 * 3.14159265 * 440.0 * static_cast<double>(i) / frecuencia));
    return m;
}

}  // namespace

PRUEBA(wav_valido_para_stt) {
    DirTemporal dir("jarvis-prueba");
    const std::string ruta = dir.archivo("ok.wav");
    VERIFICAR(escribir_wav_s16(ruta, tono(6.0, 16000, 3000.0), 16000, 1));
    InfoWav info;
    std::string error;
    VERIFICAR(leer_info_wav(ruta, info, error));
    VERIFICAR_IGUAL(info.formato, 1);
    VERIFICAR_IGUAL(info.canales, 1);
    VERIFICAR_IGUAL(info.frecuencia, 16000);
    VERIFICAR_IGUAL(info.bits, 16);
    VERIFICAR(std::fabs(info.duracion_s - 6.0) < 0.01);
    VERIFICAR(info.rms > 1000.0);
    VERIFICAR_IGUAL(validar_wav_stt(info, 5.0, 8.0, 50.0), std::string(""));
}

PRUEBA(wav_rechaza_duracion_fuera_de_rango) {
    DirTemporal dir("jarvis-prueba");
    const std::string corto = dir.archivo("corto.wav");
    const std::string largo = dir.archivo("largo.wav");
    VERIFICAR(escribir_wav_s16(corto, tono(2.0, 16000, 3000.0), 16000, 1));
    VERIFICAR(escribir_wav_s16(largo, tono(9.5, 16000, 3000.0), 16000, 1));
    InfoWav info;
    std::string error;
    VERIFICAR(leer_info_wav(corto, info, error));
    VERIFICAR(!validar_wav_stt(info, 5.0, 8.0, 50.0).empty());
    VERIFICAR(leer_info_wav(largo, info, error));
    VERIFICAR(!validar_wav_stt(info, 5.0, 8.0, 50.0).empty());
}

PRUEBA(wav_rechaza_formato_incorrecto) {
    DirTemporal dir("jarvis-prueba");
    const std::string estereo = dir.archivo("estereo.wav");
    const std::string hz44 = dir.archivo("44k.wav");
    VERIFICAR(escribir_wav_s16(estereo, tono(6.0, 16000, 3000.0), 8000, 2));
    VERIFICAR(escribir_wav_s16(hz44, tono(6.0, 44100, 3000.0), 44100, 1));
    InfoWav info;
    std::string error;
    VERIFICAR(leer_info_wav(estereo, info, error));
    VERIFICAR(validar_wav_stt(info, 5.0, 8.0, 50.0).find("mono") != std::string::npos);
    VERIFICAR(leer_info_wav(hz44, info, error));
    VERIFICAR(validar_wav_stt(info, 5.0, 8.0, 50.0).find("16000") != std::string::npos);
}

PRUEBA(wav_detecta_grabacion_vacia) {
    DirTemporal dir("jarvis-prueba");
    const std::string ruta = dir.archivo("silencio.wav");
    VERIFICAR(escribir_wav_s16(ruta, std::vector<std::int16_t>(16000 * 6, 0), 16000, 1));
    InfoWav info;
    std::string error;
    VERIFICAR(leer_info_wav(ruta, info, error));
    VERIFICAR(validar_wav_stt(info, 5.0, 8.0, 50.0).find("vacía") != std::string::npos);
}

PRUEBA(wav_rechaza_archivo_que_no_es_wav) {
    DirTemporal dir("jarvis-prueba");
    const std::string ruta = dir.archivo("falso.wav");
    VERIFICAR(escribir_archivo(ruta, "esto no es un wav"));
    InfoWav info;
    std::string error;
    VERIFICAR(!leer_info_wav(ruta, info, error));
    VERIFICAR(!error.empty());
}
