// Traduce el resultado de un proceso hijo a un FalloEtapa con diagnóstico.
#pragma once

#include <string>

#include "jarvis/estados.hpp"
#include "jarvis/proceso.hpp"

namespace jarvis {

// paso_general: sugerencia específica de la etapa cuando el proceso terminó
// con un código distinto de cero.
FalloEtapa fallo_de_proceso(Estado etapa, const ResultadoProceso& r, const std::string& binario,
                            const std::string& paso_general);

}  // namespace jarvis
