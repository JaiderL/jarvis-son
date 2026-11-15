#include "jarvis/atajos.hpp"

#include <algorithm>

namespace jarvis {

namespace {

// Convierte la letra acentuada (2 bytes UTF-8, prefijo 0xC3) a su base ASCII.
// Devuelve 0 si no es una vocal acentuada conocida.
char vocal_sin_tilde(unsigned char segundo) {
    switch (segundo) {
        case 0xA1:
        case 0x81:
        case 0xA0:
        case 0x80: return 'a';  // á Á à À
        case 0xA9:
        case 0x89:
        case 0xA8:
        case 0x88: return 'e';  // é É è È
        case 0xAD:
        case 0x8D:
        case 0xAC:
        case 0x8C: return 'i';  // í Í ì Ì
        case 0xB3:
        case 0x93:
        case 0xB2:
        case 0x92: return 'o';  // ó Ó ò Ò
        case 0xBA:
        case 0x9A:
        case 0xBC:
        case 0x9C:
        case 0xB9:
        case 0x99: return 'u';  // ú Ú ü Ü ù Ù
        default: return 0;
    }
}

// Una palabra coincide si es igual o, con tolerancia, difiere en 1 (solo
// para palabras de 4 o más letras, para no confundir "que" con "de").
bool palabra_coincide(const std::string& esperada, const std::string& dicha, bool tolerancia) {
    if (esperada == dicha) return true;
    return tolerancia && esperada.size() >= 4 && distancia_edicion(esperada, dicha) <= 1;
}

bool frases_coinciden(const std::string& esperada, const std::vector<std::string>& dichas,
                      bool tolerancia) {
    std::vector<std::string> p = separar_palabras(esperada);
    if (p.size() != dichas.size()) return false;
    for (std::size_t i = 0; i < p.size(); ++i)
        if (!palabra_coincide(p[i], dichas[i], tolerancia)) return false;
    return true;
}

bool contiene_palabra(const std::vector<std::string>& dichas, const std::string& buscada,
                      bool tolerancia) {
    return std::any_of(dichas.begin(), dichas.end(), [&](const std::string& d) {
        return palabra_coincide(buscada, d, tolerancia);
    });
}

// ¿Aparece la frase (secuencia de palabras) dentro del texto? Sin tolerancia.
bool contiene_frase(const std::string& texto, const std::string& frase) {
    if (frase.empty()) return false;
    return (" " + texto + " ").find(" " + frase + " ") != std::string::npos;
}

}  // namespace

std::string normalizar_frase(const std::string& texto) {
    std::string r;
    r.reserve(texto.size());
    auto espacio = [&r] {
        if (!r.empty() && r.back() != ' ') r += ' ';
    };
    for (std::size_t i = 0; i < texto.size(); ++i) {
        auto c = static_cast<unsigned char>(texto[i]);
        if (c < 0x80) {
            if (c >= 'A' && c <= 'Z')
                r += static_cast<char>(c - 'A' + 'a');
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                r += static_cast<char>(c);
            else
                espacio();  // signos, espacios y controles
            continue;
        }
        if (c == 0xC3 && i + 1 < texto.size()) {
            auto s = static_cast<unsigned char>(texto[i + 1]);
            if (char v = vocal_sin_tilde(s)) {
                r += v;
                ++i;
                continue;
            }
            if (s == 0xB1 || s == 0x91) {  // ñ Ñ → ñ
                r += "\xC3\xB1";
                ++i;
                continue;
            }
        }
        if (c == 0xC2 && i + 1 < texto.size()) {  // ¿ ¡ « » y otros signos latinos
            espacio();
            ++i;
            continue;
        }
        r += static_cast<char>(c);  // otros caracteres UTF-8 se conservan
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r;
}

std::vector<std::string> separar_palabras(const std::string& normalizada) {
    std::vector<std::string> v;
    std::size_t p = 0;
    while (p < normalizada.size()) {
        std::size_t fin = normalizada.find(' ', p);
        if (fin == std::string::npos) fin = normalizada.size();
        if (fin > p) v.push_back(normalizada.substr(p, fin - p));
        p = fin + 1;
    }
    return v;
}

int distancia_edicion(const std::string& a, const std::string& b) {
    std::vector<int> anterior(b.size() + 1);
    std::vector<int> actual(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) anterior[j] = static_cast<int>(j);
    for (std::size_t i = 1; i <= a.size(); ++i) {
        actual[0] = static_cast<int>(i);
        for (std::size_t j = 1; j <= b.size(); ++j) {
            int costo = a[i - 1] == b[j - 1] ? 0 : 1;
            actual[j] = std::min({anterior[j] + 1, actual[j - 1] + 1, anterior[j - 1] + costo});
        }
        std::swap(anterior, actual);
    }
    return anterior[b.size()];
}

std::string texto_hora(const std::tm& t) {
    int h = t.tm_hour % 12;
    if (h == 0) h = 12;
    std::string s = (h == 1) ? "Es la 1" : "Son las " + std::to_string(h);
    if (t.tm_min == 0)
        s += " en punto";
    else if (t.tm_min == 15)
        s += " y cuarto";
    else if (t.tm_min == 30)
        s += " y media";
    else
        s += " y " + std::to_string(t.tm_min);
    if (t.tm_hour < 6)
        s += " de la madrugada";
    else if (t.tm_hour < 12)
        s += " de la mañana";
    else if (t.tm_hour < 19)
        s += " de la tarde";
    else
        s += " de la noche";
    return s + ".";
}

std::string texto_fecha(const std::tm& t) {
    static const char* const kDias[] = {"domingo", "lunes",   "martes", "miércoles",
                                        "jueves",  "viernes", "sábado"};
    static const char* const kMeses[] = {"enero",      "febrero", "marzo",     "abril",
                                         "mayo",       "junio",   "julio",     "agosto",
                                         "septiembre", "octubre", "noviembre", "diciembre"};
    int dia = (t.tm_wday >= 0 && t.tm_wday < 7) ? t.tm_wday : 0;
    int mes = (t.tm_mon >= 0 && t.tm_mon < 12) ? t.tm_mon : 0;
    return std::string("Hoy es ") + kDias[dia] + " " + std::to_string(t.tm_mday) + " de " +
           kMeses[mes] + " de " + std::to_string(t.tm_year + 1900) + ".";
}

std::optional<Atajo> buscar_atajo(const ConfigAtajos& c, const std::string& transcripcion,
                                  const ContextoAtajo& ctx) {
    if (!c.activado) return std::nullopt;
    const std::string texto = normalizar_frase(transcripcion);
    if (texto.empty()) return std::nullopt;

    // 1. Temas bloqueados: se revisan siempre, sin importar la longitud.
    for (const auto& tema : c.temas_bloqueados) {
        if (contiene_frase(texto, tema)) return Atajo{"bloqueo", c.respuesta_bloqueo, false};
    }

    // 2. Solo frases cortas: "hola, explícame la fotosíntesis..." va al SLM.
    const std::vector<std::string> palabras = separar_palabras(texto);
    if (static_cast<int>(palabras.size()) > c.max_palabras) return std::nullopt;

    for (const auto& regla : c.reglas) {
        bool coincide = false;
        if (regla.modo == "exacta") {
            coincide = std::any_of(regla.frases.begin(), regla.frases.end(), [&](const auto& f) {
                return frases_coinciden(f, palabras, c.tolerancia_errores);
            });
        } else if (regla.modo == "palabras_clave") {
            coincide = std::all_of(regla.todas.begin(), regla.todas.end(),
                                   [&](const auto& p) {
                                       return contiene_palabra(palabras, p, c.tolerancia_errores);
                                   }) &&
                       (regla.alguna.empty() ||
                        std::any_of(regla.alguna.begin(), regla.alguna.end(), [&](const auto& p) {
                            return contiene_palabra(palabras, p, c.tolerancia_errores);
                        }));
        }
        if (!coincide) continue;

        Atajo a;
        a.id = regla.id;
        a.terminar = regla.terminar;
        std::tm local{};
        localtime_r(&ctx.ahora, &local);
        if (regla.dinamica == "hora")
            a.respuesta = texto_hora(local);
        else if (regla.dinamica == "fecha")
            a.respuesta = texto_fecha(local);
        else if (regla.dinamica == "ultima_respuesta")
            a.respuesta = ctx.ultima_respuesta.empty() ? "Todavía no he dicho nada en esta sesión."
                                                       : ctx.ultima_respuesta;
        else
            a.respuesta = regla.respuestas[ctx.rotacion % regla.respuestas.size()];
        return a;
    }
    return std::nullopt;
}

}  // namespace jarvis
