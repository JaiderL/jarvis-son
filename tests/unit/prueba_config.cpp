// Pruebas del lector JSON y de la configuración de perfiles.
#include <stdexcept>

#include "jarvis/config.hpp"
#include "jarvis/json.hpp"
#include "jarvis/util.hpp"
#include "prueba.hpp"

using namespace jarvis;

namespace {
const std::string kConfig = std::string(JARVIS_DIR_FUENTE) + "/config/config.json";
}

PRUEBA(json_parsea_tipos_basicos) {
    ValorJson v =
        parsear_json(R"({"a": 1.5, "b": "ñandú é", "c": [true, false, null], "d": {"e": -3}})");
    VERIFICAR(v.tipo == ValorJson::Tipo::Objeto);
    VERIFICAR_IGUAL(v.buscar("a")->numero, 1.5);
    VERIFICAR_IGUAL(v.buscar("b")->texto, std::string("ñandú é"));
    VERIFICAR_IGUAL(v.buscar("c")->arreglo.size(), static_cast<std::size_t>(3));
    VERIFICAR_IGUAL(v.buscar_ruta("d.e")->numero, -3.0);
    VERIFICAR(v.buscar_ruta("d.x") == nullptr);
}

PRUEBA(json_rechaza_documentos_invalidos) {
    for (const char* malo : {"{", "{\"a\":}", "[1,]", "{\"a\":1} extra", "\"sin cerrar"}) {
        bool lanzo = false;
        try {
            parsear_json(malo);
        } catch (const std::runtime_error&) {
            lanzo = true;
        }
        VERIFICAR(lanzo);
    }
}

PRUEBA(config_perfil_base_cumple_requisitos_del_docente) {
    Config c = cargar_config(kConfig, "base");
    VERIFICAR_IGUAL(c.perfil.nombre, std::string("base"));
    VERIFICAR_IGUAL(c.perfil.contexto, 1024);
    VERIFICAR_IGUAL(c.perfil.salida_tokens, 96);
    VERIFICAR_IGUAL(c.perfil.hilos, 4);
    VERIFICAR(c.perfil.modelo_stt.find("ggml-tiny.bin") != std::string::npos);
    VERIFICAR(c.perfil.modelo_slm.find("Qwen3.5-0.8B-Q4_0.gguf") != std::string::npos);
    VERIFICAR(c.perfil.voz_dir.find("es_MX") != std::string::npos);
    VERIFICAR(c.grabacion_segundos >= 5 && c.grabacion_segundos <= 8);
    VERIFICAR(c.bin_whisper.front() == '/');               // resuelto contra la raíz
    VERIFICAR_IGUAL(c.bin_ffmpeg, std::string("ffmpeg"));  // se busca en PATH
}

PRUEBA(config_perfil_alto_cumple_requisitos_del_docente) {
    Config c = cargar_config(kConfig, "alto");
    VERIFICAR_IGUAL(c.perfil.contexto, 2048);
    VERIFICAR_IGUAL(c.perfil.salida_tokens, 128);
    VERIFICAR_IGUAL(c.perfil.hilos, 6);
    VERIFICAR(c.perfil.modelo_stt.find("ggml-base.bin") != std::string::npos);
    VERIFICAR(c.perfil.modelo_slm.find("Qwen3.5-2B-Q4_K_M.gguf") != std::string::npos);
}

PRUEBA(config_perfil_inexistente_falla) {
    bool lanzo = false;
    try {
        cargar_config(kConfig, "inexistente");
    } catch (const std::runtime_error&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(config_rechaza_whisper_solo_ingles) {
    VERIFICAR(es_modelo_whisper_solo_ingles("models/ggml-tiny.en.bin"));
    VERIFICAR(!es_modelo_whisper_solo_ingles("models/ggml-tiny.bin"));
    VERIFICAR(!es_modelo_whisper_solo_ingles("models/ggml-base.bin"));
}

PRUEBA(config_resuelve_rutas) {
    VERIFICAR_IGUAL(resolver_ruta("/raiz", "/abs/bin"), std::string("/abs/bin"));
    VERIFICAR_IGUAL(resolver_ruta("/raiz", "ffmpeg"), std::string("ffmpeg"));
    VERIFICAR_IGUAL(resolver_ruta("/raiz", "models/a.bin"), std::string("/raiz/models/a.bin"));
}
