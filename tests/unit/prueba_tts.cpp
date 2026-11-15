// Verifica el argv fijo de sherpa-onnx-offline-tts: el texto es un solo argumento.
#include "comun_argv.hpp"
#include "jarvis/config.hpp"
#include "jarvis/tts.hpp"
#include "prueba.hpp"

using namespace jarvis;
using prueba::contiene;

PRUEBA(argv_sherpa_texto_es_un_solo_argumento_final) {
    Config c = cargar_config(prueba::ruta_config(), "base");
    const std::string texto = "Hola; rm -rf ~ $(id)";
    auto a = argv_sherpa(c, "/tmp/r.wav", texto);
    VERIFICAR_IGUAL(a.front(), c.bin_sherpa);
    VERIFICAR_IGUAL(a.back(), texto);
    VERIFICAR(a[1].rfind("--vits-model=", 0) == 0);
    VERIFICAR(a[1].find("es_MX") != std::string::npos);
    VERIFICAR(contiene(a, "--output-filename=/tmp/r.wav"));
}
