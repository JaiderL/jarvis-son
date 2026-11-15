#include "jarvis/captura.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>

#include "jarvis/diagnostico.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/wav.hpp"

namespace jarvis {

namespace {

const char* const kPasoTermuxApi =
    "Ejecute bash scripts/probar_api.sh. Verifique que la app Termux:API esté instalada desde "
    "la MISMA fuente que Termux (F-Droid o GitHub), que se haya abierto una vez y que tenga "
    "permiso de micrófono.";

long ms_desde(std::chrono::steady_clock::time_point t) {
    return static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t)
            .count());
}

// Espera a que el archivo exista y su tamaño deje de crecer.
bool esperar_archivo_estable(const std::string& ruta, long max_ms) {
    long long anterior = -2;
    long esperado = 0;
    while (esperado < max_ms && !g_interrumpido) {
        long long actual = tamano_archivo(ruta);
        if (actual > 0 && actual == anterior) return true;
        anterior = actual;
        dormir_ms(250);
        esperado += 250;
    }
    return tamano_archivo(ruta) > 0;
}

FalloEtapa fallo_simple(int codigo, const std::string& detalle, const std::string& paso) {
    FalloEtapa f;
    f.etapa = Estado::Grabando;
    f.codigo = codigo;
    f.detalle = detalle;
    f.siguiente_paso = paso;
    return f;
}

}  // namespace

ResultadoCaptura capturar_audio(const Config& c, const DirTemporal& dir,
                                const std::string& wav_entrada) {
    ResultadoCaptura r;
    if (!dir.valido()) {
        r.fallo = fallo_simple(1, "no se pudo crear el directorio temporal",
                               "Revise espacio libre y la variable TMPDIR.");
        return r;
    }

    std::string origen = wav_entrada;
    if (origen.empty()) {
        origen = dir.archivo("grabacion.m4a");
        const auto inicio = std::chrono::steady_clock::now();
        OpcionesProceso op;
        op.timeout_ms = c.timeout_termux_ms;
        // Inicia la grabación con límite de duración; el comando retorna enseguida.
        ResultadoProceso inicio_rec =
            ejecutar({c.bin_grabar, "-f", origen, "-l", std::to_string(c.grabacion_segundos), "-e",
                      "aac", "-c", "1"},
                     op);
        if (!inicio_rec.exito()) {
            r.fallo = fallo_de_proceso(Estado::Grabando, inicio_rec, c.bin_grabar, kPasoTermuxApi);
            return r;
        }
        if (inicio_rec.salida.find("rror") != std::string::npos) {
            r.fallo =
                fallo_simple(1, "termux-microphone-record: " + inicio_rec.salida, kPasoTermuxApi);
            return r;
        }
        std::cout << "  Hable ahora (" << c.grabacion_segundos << " s)..." << std::flush;
        dormir_ms(c.grabacion_segundos * 1000L + 600);
        // Detiene por si el límite no se aplicó; si ya terminó, solo informa.
        ejecutar({c.bin_grabar, "-q"}, op);
        std::cout << " listo.\n";
        if (g_interrumpido) {
            r.fallo = fallo_simple(130, "grabación cancelada", "Vuelva a intentarlo.");
            return r;
        }
        if (!esperar_archivo_estable(origen, 3000)) {
            r.fallo = fallo_simple(
                1, "grabación vacía: no se creó el archivo o pesa 0 bytes",
                std::string("Conceda permiso de micrófono a Termux:API (Ajustes > Apps > "
                            "Termux:API > Permisos). ") +
                    kPasoTermuxApi);
            return r;
        }
        r.ms_grabacion = ms_desde(inicio);
    } else if (!existe_archivo(origen)) {
        r.fallo = fallo_simple(2, "no existe el archivo de entrada " + origen,
                               "Revise la ruta pasada con --wav.");
        return r;
    }

    // Conversión a PCM s16le, mono, 16 kHz, recortada a la duración máxima.
    const std::string destino = dir.archivo("entrada16k.wav");
    char max_s[32];
    std::snprintf(max_s, sizeof max_s, "%.2f", c.duracion_max_s);
    OpcionesProceso op_ff;
    op_ff.timeout_ms = c.timeout_conversion_ms;
    ResultadoProceso ff =
        ejecutar({c.bin_ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-i",
                  origen, "-ac", "1", "-ar", "16000", "-c:a", "pcm_s16le", "-t", max_s, destino},
                 op_ff);
    r.ms_conversion = ff.duracion_ms;
    r.rss_ffmpeg_kb = ff.memoria_pico_kb;
    if (wav_entrada.empty()) borrar_archivo(origen);  // el original no se conserva
    if (!ff.exito()) {
        r.fallo = fallo_de_proceso(Estado::Grabando, ff, c.bin_ffmpeg,
                                   "La grabación puede estar dañada o vacía. Pruebe "
                                   "bash scripts/probar_api.sh y verifique: pkg install ffmpeg");
        return r;
    }

    InfoWav info;
    std::string error;
    if (!leer_info_wav(destino, info, error)) {
        r.fallo = fallo_simple(3, "WAV inválido: " + error, "Reinstale ffmpeg: pkg install ffmpeg");
        return r;
    }
    std::string motivo = validar_wav_stt(info, c.duracion_min_s, c.duracion_max_s, c.rms_minimo);
    if (!motivo.empty()) {
        r.fallo = fallo_simple(
            4, "audio rechazado: " + motivo,
            "Hable más cerca del micrófono durante toda la grabación. Si el RMS es ~0, el "
            "micrófono no tiene permiso u otra app lo está usando.");
        return r;
    }
    r.ok = true;
    r.wav = destino;
    r.duracion_s = info.duracion_s;
    r.rms = info.rms;
    return r;
}

}  // namespace jarvis
