// Pruebas de las respuestas rápidas (frases que no pasan por el SLM).
#include <ctime>
#include <optional>
#include <stdexcept>

#include "comun_argv.hpp"
#include "jarvis/atajos.hpp"
#include "jarvis/config.hpp"
#include "jarvis/util.hpp"
#include "prueba.hpp"

using namespace jarvis;

namespace {

ContextoAtajo contexto(std::size_t rotacion = 0) {
    ContextoAtajo c;
    c.ahora = std::time(nullptr);
    c.rotacion = rotacion;
    return c;
}

std::string id_de(const ConfigAtajos& c, const std::string& frase) {
    std::optional<Atajo> a = buscar_atajo(c, frase, contexto());
    return a ? a->id : "SLM";
}

const ConfigAtajos& atajos_reales() {
    static const ConfigAtajos a = cargar_config(prueba::ruta_config(), "base").atajos;
    return a;
}

}  // namespace

PRUEBA(atajos_normaliza_tildes_signos_y_mayusculas) {
    VERIFICAR_IGUAL(normalizar_frase("¡Hola, ¿CÓMO estás?"), std::string("hola como estas"));
    VERIFICAR_IGUAL(normalizar_frase("  Él   pidió  ÑANDÚ... "), std::string("el pidio ñandu"));
    VERIFICAR_IGUAL(normalizar_frase("¿¡!?"), std::string(""));
}

PRUEBA(atajos_distancia_de_edicion) {
    VERIFICAR_IGUAL(distancia_edicion("hola", "hola"), 0);
    VERIFICAR_IGUAL(distancia_edicion("hola", "ola"), 1);
    VERIFICAR_IGUAL(distancia_edicion("estas", "esta"), 1);
    VERIFICAR_IGUAL(distancia_edicion("gato", "pato"), 1);
    VERIFICAR_IGUAL(distancia_edicion("abc", ""), 3);
}

PRUEBA(atajos_saludos_y_frases_frecuentes_no_van_al_slm) {
    const ConfigAtajos& c = atajos_reales();
    VERIFICAR(c.activado);
    VERIFICAR_IGUAL(id_de(c, "Hola, ¿cómo estás?"), std::string("como_estas"));
    VERIFICAR_IGUAL(id_de(c, "Hola Jarvis."), std::string("saludo"));
    VERIFICAR_IGUAL(id_de(c, "¿Qué hora es?"), std::string("hora"));
    VERIFICAR_IGUAL(id_de(c, "¿Qué día es hoy?"), std::string("fecha"));
    VERIFICAR_IGUAL(id_de(c, "Muchas gracias."), std::string("gracias"));
    VERIFICAR_IGUAL(id_de(c, "Adiós."), std::string("despedida"));
}

PRUEBA(atajos_toleran_un_error_de_whisper_por_palabra) {
    const ConfigAtajos& c = atajos_reales();
    VERIFICAR_IGUAL(id_de(c, "Ola, como esta"), std::string("como_estas"));
    VERIFICAR_IGUAL(id_de(c, "gracia"), std::string("gracias"));
    // Palabras de menos de 4 letras deben coincidir exactas: "de hora es" no es "que hora es".
    VERIFICAR_IGUAL(id_de(c, "de hora es"), std::string("SLM"));
}

PRUEBA(atajos_preguntas_reales_siguen_yendo_al_slm) {
    const ConfigAtajos& c = atajos_reales();
    VERIFICAR_IGUAL(id_de(c, "Hola, explícame la fotosíntesis"), std::string("SLM"));
    VERIFICAR_IGUAL(id_de(c, "¿A qué hora abre el banco?"), std::string("SLM"));
    VERIFICAR_IGUAL(id_de(c, "¿Cuál es la capital de Colombia?"), std::string("SLM"));
    VERIFICAR_IGUAL(
        id_de(c, "hola como estas me puedes contar un chiste largo sobre programadores"),
        std::string("SLM"));
}

PRUEBA(atajos_temas_bloqueados_siempre_se_detectan) {
    const ConfigAtajos& c = atajos_reales();
    // Incluso en frases largas, que superan max_palabras.
    std::optional<Atajo> a = buscar_atajo(
        c, "Oye Jarvis, guárdame la contraseña del correo para mañana por favor", contexto());
    VERIFICAR(a.has_value());
    if (a) {
        VERIFICAR_IGUAL(a->id, std::string("bloqueo"));
        VERIFICAR(a->respuesta.find("seguridad") != std::string::npos);
    }
}

PRUEBA(atajos_respuestas_dinamicas) {
    const ConfigAtajos& c = atajos_reales();
    ContextoAtajo ctx = contexto();
    std::optional<Atajo> h = buscar_atajo(c, "que hora es", ctx);
    VERIFICAR(h && (h->respuesta.rfind("Son las", 0) == 0 || h->respuesta.rfind("Es la", 0) == 0));
    std::optional<Atajo> r = buscar_atajo(c, "repite", ctx);
    VERIFICAR(r && r->respuesta.find("Todavía no") != std::string::npos);
    ctx.ultima_respuesta = "La capital es Bogotá.";
    r = buscar_atajo(c, "repítelo", ctx);
    VERIFICAR(r && r->respuesta == "La capital es Bogotá.");
}

PRUEBA(atajos_rotan_respuestas_y_despedida_termina) {
    const ConfigAtajos& c = atajos_reales();
    std::optional<Atajo> a0 = buscar_atajo(c, "hola", contexto(0));
    std::optional<Atajo> a1 = buscar_atajo(c, "hola", contexto(1));
    VERIFICAR(a0 && a1 && a0->respuesta != a1->respuesta);
    std::optional<Atajo> d = buscar_atajo(c, "chao", contexto());
    VERIFICAR(d && d->terminar);
    VERIFICAR(a0 && !a0->terminar);
}

PRUEBA(atajos_texto_de_hora_y_fecha) {
    std::tm t{};
    t.tm_hour = 15;
    t.tm_min = 25;
    VERIFICAR_IGUAL(texto_hora(t), std::string("Son las 3 y 25 de la tarde."));
    t.tm_hour = 1;
    t.tm_min = 0;
    VERIFICAR_IGUAL(texto_hora(t), std::string("Es la 1 en punto de la madrugada."));
    t.tm_hour = 12;
    t.tm_min = 30;
    VERIFICAR_IGUAL(texto_hora(t), std::string("Son las 12 y media de la tarde."));
    t.tm_hour = 20;
    t.tm_min = 15;
    VERIFICAR_IGUAL(texto_hora(t), std::string("Son las 8 y cuarto de la noche."));
    std::tm f{};
    f.tm_wday = 0;
    f.tm_mday = 15;
    f.tm_mon = 10;
    f.tm_year = 126;
    VERIFICAR_IGUAL(texto_fecha(f), std::string("Hoy es domingo 15 de noviembre de 2026."));
}

PRUEBA(atajos_desactivados_todo_va_al_slm) {
    ConfigAtajos c = atajos_reales();
    c.activado = false;
    VERIFICAR_IGUAL(id_de(c, "hola"), std::string("SLM"));
    VERIFICAR_IGUAL(id_de(c, "mi contraseña es 123"), std::string("SLM"));
}

PRUEBA(atajos_config_invalida_se_rechaza) {
    DirTemporal dir("jarvis-prueba");
    std::string base;
    VERIFICAR(leer_archivo(prueba::ruta_config(), base));
    // Modo desconocido.
    std::string mala = base;
    std::size_t p = mala.find("\"modo\": \"exacta\"");
    VERIFICAR(p != std::string::npos);
    mala.replace(p, 16, "\"modo\": \"magica\"");
    std::string ruta = dir.archivo("config.json");
    VERIFICAR(escribir_archivo(ruta, mala));
    bool lanzo = false;
    try {
        cargar_config(ruta, "base");
    } catch (const std::runtime_error&) {
        lanzo = true;
    }
    VERIFICAR(lanzo);
}

PRUEBA(atajos_config_sin_seccion_queda_desactivada) {
    DirTemporal dir("jarvis-prueba");
    std::string base;
    VERIFICAR(leer_archivo(prueba::ruta_config(), base));
    // Renombrar la clave equivale a no tenerla (configuración vieja).
    std::size_t p = base.find("\"respuestas_rapidas\"");
    VERIFICAR(p != std::string::npos);
    base.replace(p, 20, "\"otra_seccion_vieja\"");
    std::string ruta = dir.archivo("config.json");
    VERIFICAR(escribir_archivo(ruta, base));
    Config c = cargar_config(ruta, "base");
    VERIFICAR(!c.atajos.activado);
}
