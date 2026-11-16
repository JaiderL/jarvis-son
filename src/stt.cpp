#include "jarvis/stt.hpp"

#include "jarvis/diagnostico.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/texto.hpp"

namespace jarvis {

std::vector<std::string> argv_whisper(const Config& c, const std::string& wav,
                                      const std::string& base_salida) {
    const Perfil& p = c.perfil;
    // clang-format off
    std::vector<std::string> a = {c.bin_whisper,
            "-m", p.modelo_stt,
            "-f", wav,
            "-l", "es",  // idioma fijo: español (evita que transcriba en inglés)
            "-t", std::to_string(p.hilos),
            "-bs", std::to_string(p.stt_beam),
            "-bo", std::to_string(p.stt_beam),
            "-nt",    // sin marcas de tiempo
            "-np",    // sin impresiones de diagnóstico en stdout
            "-otxt",  // escribe <base_salida>.txt
            "-of", base_salida};
    // clang-format on
    // Prompt inicial: va como un argumento separado (viene de config.json).
    if (!c.prompt_stt.empty()) {
        a.push_back("--prompt");
        a.push_back(c.prompt_stt);
    }
    return a;
}

ResultadoStt transcribir(const Config& c, const DirTemporal& dir, const std::string& wav,
                         double duracion_s) {
    ResultadoStt r;
    if (!existe_archivo(c.perfil.modelo_stt)) {
        r.fallo.etapa = Estado::Stt;
        r.fallo.codigo = 2;
        r.fallo.detalle = "no existe el modelo " + c.perfil.modelo_stt;
        r.fallo.siguiente_paso =
            "Descargue los modelos: bash scripts/descargar_modelos.sh " + c.perfil.nombre;
        return r;
    }
    const std::string base = dir.archivo("transcripcion");
    OpcionesProceso op;
    op.timeout_ms = c.timeout_stt_ms;
    ResultadoProceso p = ejecutar(argv_whisper(c, wav, base), op);
    r.ms = p.duracion_ms;
    r.rss_kb = p.memoria_pico_kb;
    r.rtf = duracion_s > 0.0 ? (static_cast<double>(p.duracion_ms) / 1000.0) / duracion_s : 0.0;
    if (!p.exito()) {
        r.fallo = fallo_de_proceso(Estado::Stt, p, c.bin_whisper,
                                   "Pruebe bash scripts/probar_stt.sh. Si el modelo no carga, "
                                   "vuelva a descargarlo (SHA-256) o recompile whisper.cpp.");
        return r;
    }
    std::string bruto;
    if (!leer_archivo(base + ".txt", bruto)) bruto = p.salida;
    borrar_archivo(base + ".txt");
    r.texto = limpiar_transcripcion(bruto);
    if (r.texto.empty()) {
        r.fallo.etapa = Estado::Stt;
        r.fallo.codigo = 5;
        r.fallo.detalle = "whisper no reconoció voz en el audio";
        r.fallo.siguiente_paso = "Hable más claro y cerca del micrófono; evite ruido de fondo.";
        return r;
    }
    r.ok = true;
    return r;
}

}  // namespace jarvis
