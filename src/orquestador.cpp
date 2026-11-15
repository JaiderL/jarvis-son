#include "jarvis/orquestador.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <utility>

#include "jarvis/captura.hpp"
#include "jarvis/slm.hpp"
#include "jarvis/stt.hpp"
#include "jarvis/tts.hpp"
#include "jarvis/util.hpp"

namespace jarvis {

Orquestador::Orquestador(Config c, bool guardar_metricas)
    : config_(std::move(c)), guardar_metricas_(guardar_metricas) {}

void Orquestador::cambiar_a(Estado nuevo) {
    if (!transicion_valida(estado_, nuevo)) {
        std::cerr << "[AVISO] transición no válida " << nombre_estado(estado_) << " -> "
                  << nombre_estado(nuevo) << "\n";
    }
    estado_ = nuevo;
    std::cout << "[" << nombre_estado(estado_) << "]\n" << std::flush;
}

bool Orquestador::fallar(const FalloEtapa& f) {
    metricas_.estado = "error";
    metricas_.etapa_error = nombre_estado(f.etapa);
    metricas_.codigo_error = f.codigo;
    estado_ = Estado::Error;
    std::cout << "[" << nombre_estado(estado_) << "]\n";
    std::cerr << formatear_fallo(f) << "\n";
    return false;
}

bool Orquestador::ejecutar_turno(int numero, const std::string& wav_entrada) {
    const auto inicio = std::chrono::steady_clock::now();
    metricas_ = MetricasTurno{};
    metricas_.fecha = ahora_iso();
    metricas_.perfil = config_.perfil.nombre;
    metricas_.turno = numero;
    estado_ = Estado::Listo;

    bool ok = false;
    {
        // El directorio temporal (audio, prompt, respuesta) se borra al salir
        // de este bloque, tanto en éxito como en error.
        DirTemporal dir("jarvis-turno");

        auto turno = [&]() -> bool {
            cambiar_a(Estado::Grabando);
            ResultadoCaptura cap = capturar_audio(config_, dir, wav_entrada);
            metricas_.ms_grabacion = cap.ms_grabacion;
            metricas_.ms_conversion = cap.ms_conversion;
            metricas_.rss_ffmpeg_kb = cap.rss_ffmpeg_kb;
            metricas_.duracion_audio_s = cap.duracion_s;
            if (!cap.ok) return fallar(cap.fallo);

            cambiar_a(Estado::Stt);
            ResultadoStt stt = transcribir(config_, dir, cap.wav, cap.duracion_s);
            borrar_archivo(cap.wav);  // el audio no se conserva
            metricas_.ms_stt = stt.ms;
            metricas_.rss_stt_kb = stt.rss_kb;
            metricas_.rtf_stt = stt.rtf;
            if (!stt.ok) return fallar(stt.fallo);
            std::cout << "  Usted: " << stt.texto << "\n";

            cambiar_a(Estado::Slm);
            ResultadoSlm slm = generar_respuesta(config_, dir, stt.texto);
            metricas_.ms_slm = slm.ms;
            metricas_.rss_slm_kb = slm.rss_kb;
            if (!slm.ok) return fallar(slm.fallo);
            std::cout << "  Jarvis: " << slm.texto << "\n";

            cambiar_a(Estado::Tts);
            ResultadoTts tts = sintetizar_y_reproducir(config_, dir, slm.texto);
            metricas_.ms_tts = tts.ms_sintesis;
            metricas_.ms_reproduccion = tts.ms_reproduccion;
            metricas_.rss_tts_kb = tts.rss_kb;
            metricas_.motor_tts = tts.motor;
            if (!tts.ok) return fallar(tts.fallo);
            return true;
        };
        ok = turno();
    }

    metricas_.ms_total = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - inicio)
                                               .count());
    if (ok) metricas_.estado = "ok";
    if (guardar_metricas_ && !anexar_metricas(config_.archivo_metricas, metricas_))
        std::cerr << "[AVISO] no se pudo escribir " << config_.archivo_metricas << "\n";

    char resumen[256];
    std::snprintf(resumen, sizeof resumen,
                  "  Tiempos (ms): grabación %ld | conversión %ld | STT %ld (RTF %.2f) | SLM %ld | "
                  "TTS %ld | reproducción %ld | total %ld\n",
                  metricas_.ms_grabacion, metricas_.ms_conversion, metricas_.ms_stt,
                  metricas_.rtf_stt, metricas_.ms_slm, metricas_.ms_tts, metricas_.ms_reproduccion,
                  metricas_.ms_total);
    std::cout << resumen;
    std::snprintf(resumen, sizeof resumen,
                  "  Memoria pico (MB): ffmpeg %.0f | STT %.0f | SLM %.0f | TTS %.0f\n",
                  metricas_.rss_ffmpeg_kb / 1024.0, metricas_.rss_stt_kb / 1024.0,
                  metricas_.rss_slm_kb / 1024.0, metricas_.rss_tts_kb / 1024.0);
    std::cout << resumen;

    if (ok)
        cambiar_a(Estado::Listo);
    else
        estado_ = Estado::Listo;  // ERROR → LISTO para el siguiente turno
    return ok;
}

}  // namespace jarvis
