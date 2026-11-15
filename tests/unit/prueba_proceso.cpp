// Pruebas del lanzador de procesos (fork/execvp, códigos de salida, timeouts).
#include "jarvis/proceso.hpp"
#include "jarvis/util.hpp"
#include "prueba.hpp"

using namespace jarvis;

PRUEBA(proceso_captura_stdout_y_codigo_cero) {
    ResultadoProceso r = ejecutar({"echo", "hola", "mundo"});
    VERIFICAR(r.lanzado);
    VERIFICAR(r.exito());
    VERIFICAR_IGUAL(r.salida, std::string("hola mundo\n"));
    VERIFICAR(r.memoria_pico_kb > 0);
}

PRUEBA(proceso_reporta_codigo_distinto_de_cero) {
    ResultadoProceso r = ejecutar({"false"});
    VERIFICAR(r.lanzado);
    VERIFICAR(!r.exito());
    VERIFICAR_IGUAL(r.codigo_salida, 1);
    VERIFICAR_IGUAL(r.codigo_reporte(), 1);
}

PRUEBA(proceso_binario_inexistente_no_se_lanza) {
    ResultadoProceso r = ejecutar({"binario-que-no-existe-jarvis"});
    VERIFICAR(!r.lanzado);
    VERIFICAR_IGUAL(r.codigo_reporte(), 127);
}

PRUEBA(proceso_respeta_timeout) {
    OpcionesProceso op;
    op.timeout_ms = 300;
    ResultadoProceso r = ejecutar({"sleep", "5"}, op);
    VERIFICAR(r.timeout);
    VERIFICAR_IGUAL(r.codigo_reporte(), 124);
    VERIFICAR(r.duracion_ms < 4000);
}

PRUEBA(proceso_captura_stderr) {
    ResultadoProceso r = ejecutar({"ls", "/ruta/que/no/existe/jarvis"});
    VERIFICAR(r.lanzado);
    VERIFICAR(!r.exito());
    VERIFICAR(!r.error.empty());
}

// Seguridad: los metacaracteres de shell llegan literales al programa;
// no hay ningún shell que los interprete.
PRUEBA(proceso_no_interpreta_metacaracteres_de_shell) {
    const std::string peligroso = "hola; rm -rf ~ && $(reboot) `id` | cat > /tmp/x";
    ResultadoProceso r = ejecutar({"printf", "%s", peligroso});
    VERIFICAR(r.exito());
    VERIFICAR_IGUAL(r.salida, peligroso);
}

PRUEBA(proceso_lee_stdin_desde_archivo) {
    DirTemporal dir("jarvis-prueba");
    const std::string archivo = dir.archivo("entrada.txt");
    VERIFICAR(escribir_archivo(archivo, "texto por stdin"));
    OpcionesProceso op;
    op.archivo_stdin = archivo;
    ResultadoProceso r = ejecutar({"cat"}, op);
    VERIFICAR(r.exito());
    VERIFICAR_IGUAL(r.salida, std::string("texto por stdin"));
}

PRUEBA(proceso_stdin_por_defecto_es_dev_null) {
    OpcionesProceso op;
    op.timeout_ms = 3000;
    ResultadoProceso r = ejecutar({"cat"}, op);
    VERIFICAR(r.exito());
    VERIFICAR(!r.timeout);
    VERIFICAR(r.salida.empty());
}

PRUEBA(dir_temporal_se_borra_al_destruirse) {
    std::string ruta;
    {
        DirTemporal dir("jarvis-prueba");
        VERIFICAR(dir.valido());
        ruta = dir.ruta();
        VERIFICAR(escribir_archivo(dir.archivo("a.wav"), "x"));
        VERIFICAR(existe_directorio(ruta));
    }
    VERIFICAR(!existe_directorio(ruta));
}
