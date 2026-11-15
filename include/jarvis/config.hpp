// Configuración del asistente y perfiles de hardware (config/config.json).
#pragma once

#include <string>
#include <vector>

#include "jarvis/atajos.hpp"

namespace jarvis {

struct Perfil {
    std::string nombre;
    std::string modelo_stt;  // ruta absoluta al ggml de whisper (multilingüe)
    int stt_beam = 1;
    std::string modelo_slm;  // ruta absoluta al GGUF
    int contexto = 1024;
    int salida_tokens = 96;
    int hilos = 4;
    std::string voz_dir;     // carpeta de la voz Piper empaquetada para sherpa-onnx
    std::string voz_modelo;  // nombre del .onnx dentro de voz_dir
};

struct Config {
    std::string raiz;  // raíz del proyecto (padre de config/)
    std::string ruta_config;

    // Binarios (rutas absolutas o nombres buscados en PATH).
    std::string bin_whisper;
    std::string bin_llama;
    std::string bin_sherpa;
    std::string bin_ffmpeg;
    std::string bin_grabar;
    std::string bin_reproducir;
    std::string bin_tts_respaldo;

    // Grabación.
    int grabacion_segundos = 6;
    double duracion_min_s = 5.0;
    double duracion_max_s = 8.0;
    double rms_minimo = 50.0;
    int timeout_termux_ms = 15000;

    // Tiempos límite por etapa (ms).
    int timeout_conversion_ms = 30000;
    int timeout_stt_ms = 120000;
    int timeout_slm_ms = 240000;
    int timeout_tts_ms = 90000;
    int timeout_reproduccion_ms = 60000;

    // SLM.
    double temperatura = 0.3;
    std::string prompt_sistema;
    std::vector<std::string> args_extra_llama;

    // TTS.
    std::string motor_tts = "sherpa";  // "sherpa" o "termux" (solo demostración)
    bool permitir_respaldo_termux = false;

    // Respuestas rápidas sin SLM (sección opcional "respuestas_rapidas").
    ConfigAtajos atajos;

    std::string archivo_metricas;

    Perfil perfil;
};

// Carga y valida la configuración. perfil vacío = "perfil_activo" del JSON.
// Lanza std::runtime_error con un mensaje en español si algo es inválido.
Config cargar_config(const std::string& ruta, const std::string& perfil);

// Resuelve rutas: absolutas se respetan, "~/" usa $HOME, las que contienen
// '/' son relativas a la raíz y los nombres simples se buscan en PATH.
std::string resolver_ruta(const std::string& raiz, const std::string& ruta);

// true si el nombre del modelo whisper es solo inglés (*.en.bin), prohibido.
bool es_modelo_whisper_solo_ingles(const std::string& ruta);

}  // namespace jarvis
