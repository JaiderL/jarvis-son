#include "jarvis/slm.hpp"

#include <cstdio>

#include "jarvis/diagnostico.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/texto.hpp"

namespace jarvis {

std::string construir_prompt_chatml(const std::string& sistema, const std::string& usuario) {
    // Formato ChatML de Qwen3.5. El bloque <think> vacío al inicio de la
    // respuesta es lo mismo que genera la plantilla oficial con
    // enable_thinking=false: el modelo responde directo, sin razonar.
    return "<|im_start|>system\n" + quitar_marcas_especiales(sistema) + "<|im_end|>\n" +
           "<|im_start|>user\n" + quitar_marcas_especiales(sanear_texto(usuario, 1000)) +
           "<|im_end|>\n" + "<|im_start|>assistant\n<think>\n\n</think>\n\n";
}

std::vector<std::string> argv_llama(const Config& c, const std::string& archivo_prompt) {
    const Perfil& p = c.perfil;
    char temp[16];
    std::snprintf(temp, sizeof temp, "%.2f", c.temperatura);
    // clang-format off
    std::vector<std::string> a = {c.bin_llama,
                                  "-m", p.modelo_slm,
                                  "-c", std::to_string(p.contexto),
                                  "-n", std::to_string(p.salida_tokens),
                                  "-t", std::to_string(p.hilos),
                                  "--temp", temp,
                                  "-f", archivo_prompt,  // prompt completo por ARCHIVO
                                  "-no-cnv",             // una sola ejecución, sin chat interactivo
                                  "--no-display-prompt",
                                  "--simple-io",
                                  "--no-warmup"};
    // clang-format on
    // Argumentos extra validados (para ajustar a futuras versiones de llama.cpp).
    for (const auto& extra : c.args_extra_llama) a.push_back(extra);
    return a;
}

ResultadoSlm generar_respuesta(const Config& c, const DirTemporal& dir,
                               const std::string& texto_usuario) {
    ResultadoSlm r;
    r.fallo.etapa = Estado::Slm;
    if (!existe_archivo(c.perfil.modelo_slm)) {
        r.fallo.codigo = 2;
        r.fallo.detalle = "no existe el modelo " + c.perfil.modelo_slm;
        r.fallo.siguiente_paso =
            "Descargue los modelos: bash scripts/descargar_modelos.sh " + c.perfil.nombre;
        return r;
    }
    const std::string archivo_prompt = dir.archivo("prompt.txt");
    if (!escribir_archivo(archivo_prompt,
                          construir_prompt_chatml(c.prompt_sistema, texto_usuario))) {
        r.fallo.codigo = 1;
        r.fallo.detalle = "no se pudo escribir el archivo temporal del prompt";
        r.fallo.siguiente_paso = "Revise el espacio libre del teléfono.";
        return r;
    }
    OpcionesProceso op;
    op.timeout_ms = c.timeout_slm_ms;
    ResultadoProceso p = ejecutar(argv_llama(c, archivo_prompt), op);
    borrar_archivo(archivo_prompt);
    r.ms = p.duracion_ms;
    r.rss_kb = p.memoria_pico_kb;
    if (!p.exito()) {
        r.fallo = fallo_de_proceso(
            Estado::Slm, p, c.bin_llama,
            "Pruebe bash scripts/probar_slm.sh. Si dice 'unknown model architecture' o "
            "'invalid argument', recompile llama.cpp con la versión fijada en "
            "scripts/instalar.sh o revise slm.args_extra_llama en config/config.json.");
        return r;
    }
    r.texto = limpiar_respuesta_slm(p.salida);
    if (r.texto.empty()) {
        r.fallo.codigo = 6;
        r.fallo.detalle = "el modelo no generó texto utilizable";
        r.fallo.siguiente_paso = "Pruebe bash scripts/probar_slm.sh y revise la salida completa.";
        return r;
    }
    r.ok = true;
    return r;
}

}  // namespace jarvis
