// Etapa SLM: llama.cpp (llama-completion) en una sola ejecución por turno.
#pragma once

#include <string>
#include <vector>

#include "jarvis/config.hpp"
#include "jarvis/estados.hpp"
#include "jarvis/util.hpp"

namespace jarvis {

struct ResultadoSlm {
    bool ok = false;
    FalloEtapa fallo;
    std::string texto;
    long ms = 0;
    long rss_kb = 0;
};

// Prompt ChatML (Qwen3.5) con el modo "thinking" desactivado: el turno del
// asistente empieza con un bloque <think></think> vacío, igual que la plantilla
// oficial con enable_thinking=false. Quita marcas especiales del texto del usuario.
std::string construir_prompt_chatml(const std::string& sistema, const std::string& usuario);

// argv exacto de llama-completion. El prompt (con el texto del usuario) va en
// un ARCHIVO (-f): nunca en la línea de comandos ni en un shell.
std::vector<std::string> argv_llama(const Config& c, const std::string& archivo_prompt);

ResultadoSlm generar_respuesta(const Config& c, const DirTemporal& dir,
                               const std::string& texto_usuario);

}  // namespace jarvis
