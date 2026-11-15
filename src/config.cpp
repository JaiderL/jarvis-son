#include "jarvis/config.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "jarvis/json.hpp"
#include "jarvis/util.hpp"

namespace fs = std::filesystem;

namespace jarvis {

namespace {

[[noreturn]] void fallar(const std::string& msg) {
    throw std::runtime_error("config/config.json: " + msg);
}

const ValorJson& requerir(const ValorJson& raiz, const std::string& ruta, ValorJson::Tipo tipo) {
    const ValorJson* v = raiz.buscar_ruta(ruta);
    if (v == nullptr) fallar("falta la clave \"" + ruta + "\"");
    if (v->tipo != tipo) fallar("la clave \"" + ruta + "\" tiene un tipo incorrecto");
    return *v;
}

std::string texto(const ValorJson& raiz, const std::string& ruta) {
    const std::string& s = requerir(raiz, ruta, ValorJson::Tipo::Texto).texto;
    if (s.empty()) fallar("la clave \"" + ruta + "\" no puede estar vacía");
    return s;
}

int entero(const ValorJson& raiz, const std::string& ruta, int minimo, int maximo) {
    double d = requerir(raiz, ruta, ValorJson::Tipo::Numero).numero;
    if (std::floor(d) != d || d < minimo || d > maximo)
        fallar("\"" + ruta + "\" debe ser un entero entre " + std::to_string(minimo) + " y " +
               std::to_string(maximo));
    return static_cast<int>(d);
}

double real(const ValorJson& raiz, const std::string& ruta, double minimo, double maximo) {
    double d = requerir(raiz, ruta, ValorJson::Tipo::Numero).numero;
    if (d < minimo || d > maximo)
        fallar("\"" + ruta + "\" fuera de rango [" + std::to_string(minimo) + ", " +
               std::to_string(maximo) + "]");
    return d;
}

bool booleano(const ValorJson& raiz, const std::string& ruta) {
    return requerir(raiz, ruta, ValorJson::Tipo::Booleano).booleano;
}

// Los argumentos extra de llama.cpp solo pueden tener caracteres seguros;
// igual se pasan como argv separados (nunca por shell).
bool argumento_seguro(const std::string& a) {
    if (a.empty() || a.size() > 64) return false;
    for (char c : a) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_' || c == '.' || c == '=' || c == ':' || c == ',' ||
                  c == '{' || c == '}' || c == '"';
        if (!ok) return false;
    }
    return true;
}

bool nombre_perfil_valido(const std::string& p) {
    if (p.empty() || p.size() > 32) return false;
    for (char c : p)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

// Lista de textos no vacíos (cada uno <= max bytes); se normaliza si se pide.
std::vector<std::string> lista_textos(const ValorJson& obj, const std::string& clave,
                                      const std::string& contexto, bool normalizar,
                                      std::size_t max = 300) {
    std::vector<std::string> v;
    const ValorJson* a = obj.buscar(clave);
    if (a == nullptr) return v;
    if (a->tipo != ValorJson::Tipo::Arreglo)
        fallar(contexto + "." + clave + " debe ser un arreglo");
    for (const auto& e : a->arreglo) {
        if (e.tipo != ValorJson::Tipo::Texto || e.texto.empty() || e.texto.size() > max)
            fallar(contexto + "." + clave + " contiene un texto vacío o demasiado largo");
        std::string t = normalizar ? normalizar_frase(e.texto) : e.texto;
        if (t.empty()) fallar(contexto + "." + clave + " contiene un texto sin letras");
        v.push_back(t);
    }
    return v;
}

// Sección opcional "respuestas_rapidas". Si no existe, los atajos quedan
// desactivados y todo va al SLM (compatibilidad con configuraciones viejas).
ConfigAtajos cargar_atajos(const ValorJson& j) {
    ConfigAtajos a;
    const ValorJson* s = j.buscar("respuestas_rapidas");
    if (s == nullptr) return a;
    if (s->tipo != ValorJson::Tipo::Objeto) fallar("\"respuestas_rapidas\" debe ser un objeto");
    a.activado = booleano(j, "respuestas_rapidas.activado");
    a.max_palabras = entero(j, "respuestas_rapidas.max_palabras", 1, 15);
    a.tolerancia_errores = booleano(j, "respuestas_rapidas.tolerancia_errores");

    const ValorJson* reglas = s->buscar("reglas");
    if (reglas == nullptr || reglas->tipo != ValorJson::Tipo::Arreglo)
        fallar("\"respuestas_rapidas.reglas\" debe ser un arreglo");
    for (std::size_t i = 0; i < reglas->arreglo.size(); ++i) {
        const ValorJson& r = reglas->arreglo[i];
        const std::string ctx = "respuestas_rapidas.reglas[" + std::to_string(i) + "]";
        if (r.tipo != ValorJson::Tipo::Objeto) fallar(ctx + " debe ser un objeto");
        ReglaAtajo regla;
        const ValorJson* id = r.buscar("id");
        const ValorJson* modo = r.buscar("modo");
        if (id == nullptr || id->tipo != ValorJson::Tipo::Texto || id->texto.empty())
            fallar(ctx + " necesita un \"id\"");
        if (modo == nullptr || modo->tipo != ValorJson::Tipo::Texto)
            fallar(ctx + " necesita un \"modo\"");
        regla.id = id->texto;
        regla.modo = modo->texto;
        regla.frases = lista_textos(r, "frases", ctx, true);
        regla.todas = lista_textos(r, "todas", ctx, true);
        regla.alguna = lista_textos(r, "alguna", ctx, true);
        regla.respuestas = lista_textos(r, "respuestas", ctx, false);
        if (const ValorJson* d = r.buscar("respuesta_dinamica")) {
            if (d->tipo != ValorJson::Tipo::Texto ||
                (d->texto != "hora" && d->texto != "fecha" && d->texto != "ultima_respuesta"))
                fallar(ctx + ".respuesta_dinamica debe ser hora, fecha o ultima_respuesta");
            regla.dinamica = d->texto;
        }
        if (const ValorJson* t = r.buscar("terminar")) {
            if (t->tipo != ValorJson::Tipo::Booleano) fallar(ctx + ".terminar debe ser booleano");
            regla.terminar = t->booleano;
        }
        if (regla.modo == "exacta") {
            if (regla.frases.empty()) fallar(ctx + " (modo exacta) necesita \"frases\"");
        } else if (regla.modo == "palabras_clave") {
            if (regla.todas.empty()) fallar(ctx + " (modo palabras_clave) necesita \"todas\"");
        } else {
            fallar(ctx + ".modo debe ser \"exacta\" o \"palabras_clave\"");
        }
        if (regla.respuestas.empty() && regla.dinamica.empty())
            fallar(ctx + " necesita \"respuestas\" o \"respuesta_dinamica\"");
        a.reglas.push_back(regla);
    }

    if (const ValorJson* b = s->buscar("temas_bloqueados")) {
        if (b->tipo != ValorJson::Tipo::Objeto)
            fallar("respuestas_rapidas.temas_bloqueados debe ser un objeto");
        a.temas_bloqueados =
            lista_textos(*b, "frases", "respuestas_rapidas.temas_bloqueados", true);
        if (!a.temas_bloqueados.empty())
            a.respuesta_bloqueo = texto(j, "respuestas_rapidas.temas_bloqueados.respuesta");
    }
    return a;
}

}  // namespace

std::string resolver_ruta(const std::string& raiz, const std::string& ruta) {
    if (ruta.empty()) return ruta;
    if (ruta[0] == '/') return ruta;
    if (ruta.rfind("~/", 0) == 0) {
        const char* home = std::getenv("HOME");
        return std::string(home != nullptr ? home : "") + ruta.substr(1);
    }
    if (ruta.find('/') == std::string::npos) return ruta;  // se busca en PATH
    return (fs::path(raiz) / ruta).lexically_normal().string();
}

bool es_modelo_whisper_solo_ingles(const std::string& ruta) {
    std::string nombre = fs::path(ruta).filename().string();
    return nombre.find(".en.") != std::string::npos || nombre.find(".en-") != std::string::npos;
}

Config cargar_config(const std::string& ruta, const std::string& perfil_pedido) {
    std::string contenido;
    if (!leer_archivo(ruta, contenido)) fallar("no se pudo leer " + ruta);
    ValorJson j = parsear_json(contenido);

    Config c;
    std::error_code ec;
    fs::path absoluta = fs::absolute(ruta, ec);
    c.ruta_config = absoluta.string();
    c.raiz = absoluta.parent_path().parent_path().string();
    auto r = [&](const std::string& clave) { return resolver_ruta(c.raiz, texto(j, clave)); };

    c.bin_whisper = r("binarios.whisper");
    c.bin_llama = r("binarios.llama");
    c.bin_sherpa = r("binarios.sherpa_tts");
    c.bin_ffmpeg = r("binarios.ffmpeg");
    c.bin_grabar = r("binarios.grabar");
    c.bin_reproducir = r("binarios.reproducir");
    c.bin_tts_respaldo = r("binarios.tts_respaldo");

    c.grabacion_segundos = entero(j, "grabacion.segundos", 5, 8);
    c.duracion_min_s = real(j, "grabacion.duracion_min_s", 1.0, 8.0);
    c.duracion_max_s = real(j, "grabacion.duracion_max_s", c.duracion_min_s, 8.0);
    c.rms_minimo = real(j, "grabacion.rms_minimo", 0.0, 10000.0);
    c.timeout_termux_ms = entero(j, "grabacion.timeout_termux_s", 3, 60) * 1000;

    c.timeout_conversion_ms = entero(j, "tiempos_limite_s.conversion", 5, 600) * 1000;
    c.timeout_stt_ms = entero(j, "tiempos_limite_s.stt", 10, 1800) * 1000;
    c.timeout_slm_ms = entero(j, "tiempos_limite_s.slm", 10, 1800) * 1000;
    c.timeout_tts_ms = entero(j, "tiempos_limite_s.tts", 10, 1800) * 1000;
    c.timeout_reproduccion_ms = entero(j, "tiempos_limite_s.reproduccion", 5, 600) * 1000;

    c.temperatura = real(j, "slm.temperatura", 0.0, 2.0);
    c.prompt_sistema = texto(j, "slm.prompt_sistema");
    for (const auto& a : requerir(j, "slm.args_extra_llama", ValorJson::Tipo::Arreglo).arreglo) {
        if (a.tipo != ValorJson::Tipo::Texto || !argumento_seguro(a.texto))
            fallar("\"slm.args_extra_llama\" contiene un argumento no permitido");
        c.args_extra_llama.push_back(a.texto);
    }

    c.motor_tts = texto(j, "tts.motor");
    if (c.motor_tts != "sherpa" && c.motor_tts != "termux")
        fallar("\"tts.motor\" debe ser \"sherpa\" o \"termux\"");
    c.permitir_respaldo_termux = booleano(j, "tts.permitir_respaldo_termux");

    c.atajos = cargar_atajos(j);

    c.archivo_metricas = resolver_ruta(c.raiz, texto(j, "metricas.archivo"));
    if (c.archivo_metricas.find('/') == std::string::npos)
        c.archivo_metricas = (fs::path(c.raiz) / c.archivo_metricas).string();

    std::string nombre = perfil_pedido.empty() ? texto(j, "perfil_activo") : perfil_pedido;
    if (!nombre_perfil_valido(nombre)) fallar("nombre de perfil inválido: " + nombre);
    const std::string pre = "perfiles." + nombre + ".";
    if (j.buscar_ruta("perfiles." + nombre) == nullptr) fallar("no existe el perfil " + nombre);

    Perfil& p = c.perfil;
    p.nombre = nombre;
    p.modelo_stt = r(pre + "modelo_stt");
    if (es_modelo_whisper_solo_ingles(p.modelo_stt))
        fallar("el modelo de whisper debe ser MULTILINGÜE (no *.en.bin)");
    p.stt_beam = entero(j, pre + "stt_beam", 1, 8);
    p.modelo_slm = r(pre + "modelo_slm");
    p.contexto = entero(j, pre + "contexto", 256, 8192);
    p.salida_tokens = entero(j, pre + "salida_tokens", 16, 512);
    p.hilos = entero(j, pre + "hilos", 1, 8);
    p.voz_dir = r(pre + "voz_dir");
    p.voz_modelo = texto(j, pre + "voz_modelo");
    if (p.voz_modelo.find('/') != std::string::npos)
        fallar("\"voz_modelo\" debe ser solo un nombre de archivo");
    return c;
}

}  // namespace jarvis
