// Verifica el argv fijo de whisper-cli (español forzado, sin shell).
#include "comun_argv.hpp"
#include "jarvis/config.hpp"
#include "jarvis/stt.hpp"
#include "prueba.hpp"

using namespace jarvis;
using prueba::contiene;
using prueba::siguiente;

PRUEBA(argv_whisper_fuerza_espanol) {
    Config c = cargar_config(prueba::ruta_config(), "base");
    auto a = argv_whisper(c, "/tmp/x.wav", "/tmp/salida");
    VERIFICAR_IGUAL(a.front(), c.bin_whisper);
    VERIFICAR_IGUAL(siguiente(a, "-l"), std::string("es"));
    VERIFICAR_IGUAL(siguiente(a, "-t"), std::string("4"));
    VERIFICAR_IGUAL(siguiente(a, "-f"), std::string("/tmp/x.wav"));
    VERIFICAR(contiene(a, "-otxt"));
}

PRUEBA(argv_whisper_perfil_alto_usa_modelo_base) {
    Config c = cargar_config(prueba::ruta_config(), "alto");
    auto a = argv_whisper(c, "/tmp/x.wav", "/tmp/salida");
    VERIFICAR(siguiente(a, "-m").find("ggml-base.bin") != std::string::npos);
    VERIFICAR_IGUAL(siguiente(a, "-t"), std::string("6"));
}
