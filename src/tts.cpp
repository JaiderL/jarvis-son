#include "jarvis/tts.hpp"

#include "jarvis/diagnostico.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/texto.hpp"
#include "jarvis/wav.hpp"

namespace jarvis {

namespace {

ResultadoTts tts_termux(const Config& c, const DirTemporal& dir, const std::string& texto) {
    ResultadoTts r;
    r.motor = "termux";
    // El texto se entrega por stdin (archivo), no como argumento.
    const std::string archivo = dir.archivo("respuesta.txt");
    if (!escribir_archivo(archivo, texto)) {
        r.fallo.etapa = Estado::Tts;
        r.fallo.codigo = 1;
        r.fallo.detalle = "no se pudo escribir el texto temporal";
        r.fallo.siguiente_paso = "Revise el espacio libre del teléfono.";
        return r;
    }
    OpcionesProceso op;
    op.timeout_ms = c.timeout_tts_ms;
    op.archivo_stdin = archivo;
    ResultadoProceso p = ejecutar({c.bin_tts_respaldo, "-l", "es"}, op);
    borrar_archivo(archivo);
    r.ms_reproduccion = p.duracion_ms;
    r.rss_kb = p.memoria_pico_kb;
    if (!p.exito()) {
        r.fallo = fallo_de_proceso(Estado::Tts, p, c.bin_tts_respaldo,
                                   "Ejecute bash scripts/probar_api.sh y revise que exista un "
                                   "motor de voz del sistema en español.");
        return r;
    }
    r.ok = true;
    return r;
}

}  // namespace

std::vector<std::string> argv_sherpa(const Config& c, const std::string& wav_salida,
                                     const std::string& texto) {
    const Perfil& p = c.perfil;
    return {c.bin_sherpa,
            "--vits-model=" + p.voz_dir + "/" + p.voz_modelo,
            "--vits-tokens=" + p.voz_dir + "/tokens.txt",
            "--vits-data-dir=" + p.voz_dir + "/espeak-ng-data",
            "--num-threads=" + std::to_string(p.hilos),
            "--output-filename=" + wav_salida,
            texto};
}

ResultadoTts sintetizar_y_reproducir(const Config& c, const DirTemporal& dir,
                                     const std::string& texto_bruto) {
    const std::string texto = sanear_texto(texto_bruto, 600);
    ResultadoTts r;
    r.fallo.etapa = Estado::Tts;
    if (texto.empty()) {
        r.fallo.codigo = 7;
        r.fallo.detalle = "no hay texto para sintetizar";
        r.fallo.siguiente_paso = "Revise la salida de la etapa SLM.";
        return r;
    }
    if (c.motor_tts == "termux") return tts_termux(c, dir, texto);

    r.motor = "sherpa";
    const std::string modelo = c.perfil.voz_dir + "/" + c.perfil.voz_modelo;
    if (!existe_archivo(modelo)) {
        r.fallo.codigo = 2;
        r.fallo.detalle = "no existe la voz " + modelo;
        r.fallo.siguiente_paso =
            "Descargue los modelos: bash scripts/descargar_modelos.sh " + c.perfil.nombre;
        if (c.permitir_respaldo_termux) return tts_termux(c, dir, texto);
        return r;
    }

    const std::string wav = dir.archivo("respuesta.wav");
    OpcionesProceso op;
    op.timeout_ms = c.timeout_tts_ms;
    ResultadoProceso p = ejecutar(argv_sherpa(c, wav, texto), op);
    r.ms_sintesis = p.duracion_ms;
    r.rss_kb = p.memoria_pico_kb;
    InfoWav info;
    std::string error;
    if (!p.exito() || !leer_info_wav(wav, info, error) || info.duracion_s <= 0.0) {
        if (p.exito()) {
            r.fallo.codigo = 8;
            r.fallo.detalle = "sherpa-onnx no produjo un WAV válido: " + error;
            r.fallo.siguiente_paso = "Pruebe bash scripts/probar_tts.sh";
        } else {
            r.fallo = fallo_de_proceso(Estado::Tts, p, c.bin_sherpa,
                                       "Pruebe bash scripts/probar_tts.sh. Revise que la carpeta "
                                       "de la voz tenga tokens.txt y espeak-ng-data/.");
        }
        if (c.permitir_respaldo_termux) return tts_termux(c, dir, texto);
        return r;
    }

    // Reproducción síncrona (play-audio bloquea hasta terminar).
    OpcionesProceso op_rep;
    op_rep.timeout_ms = c.timeout_reproduccion_ms;
    ResultadoProceso rep = ejecutar({c.bin_reproducir, wav}, op_rep);
    r.ms_reproduccion = rep.duracion_ms;
    borrar_archivo(wav);
    if (!rep.exito()) {
        r.fallo = fallo_de_proceso(Estado::Tts, rep, c.bin_reproducir,
                                   "Instale el reproductor (pkg install play-audio), suba el "
                                   "volumen multimedia y pruebe bash scripts/probar_tts.sh");
        return r;
    }
    r.ok = true;
    return r;
}

}  // namespace jarvis
