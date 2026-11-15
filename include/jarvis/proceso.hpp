// Lanzador de procesos seguro: fork + execvp con argv fijo.
// Nunca usa las funciones system ni popen, ni un shell intermedio.
#pragma once

#include <csignal>
#include <cstddef>
#include <string>
#include <vector>

namespace jarvis {

// Bandera global que se activa con SIGINT/SIGTERM; el lanzador la revisa
// para terminar al proceso hijo de forma ordenada.
extern volatile std::sig_atomic_t g_interrumpido;

// Instala manejadores de SIGINT y SIGTERM que activan g_interrumpido.
void instalar_manejador_senales();

struct OpcionesProceso {
    int timeout_ms = 60000;             // tiempo máximo de ejecución
    std::string archivo_stdin;          // vacío = /dev/null
    std::size_t max_captura = 1 << 20;  // bytes máximos guardados por flujo
};

struct ResultadoProceso {
    bool lanzado = false;  // execvp tuvo éxito
    int errno_lanzamiento = 0;
    int codigo_salida = -1;  // WEXITSTATUS si terminó normalmente
    int senal = 0;           // señal que terminó al proceso, si aplica
    bool timeout = false;
    bool interrumpido = false;
    std::string salida;  // stdout capturado
    std::string error;   // stderr capturado
    long duracion_ms = 0;
    long memoria_pico_kb = 0;  // rusage.ru_maxrss (KB en Linux/Android)

    bool exito() const {
        return lanzado && !timeout && !interrumpido && senal == 0 && codigo_salida == 0;
    }
    // Código numérico para reportes: salida, 128+señal, 124 timeout, 127 no lanzado.
    int codigo_reporte() const;
    std::string describir() const;
};

// Ejecuta argv[0] (buscado en PATH si no contiene '/') con los argumentos
// exactos de argv. Captura stdout/stderr, mide tiempo y memoria pico (wait4).
ResultadoProceso ejecutar(const std::vector<std::string>& argv, const OpcionesProceso& op = {});

}  // namespace jarvis
