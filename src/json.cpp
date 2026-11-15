#include "jarvis/json.hpp"

#include <cstdlib>
#include <stdexcept>

namespace jarvis {

const ValorJson* ValorJson::buscar(const std::string& clave) const {
    if (tipo != Tipo::Objeto) return nullptr;
    for (const auto& par : objeto)
        if (par.first == clave) return &par.second;
    return nullptr;
}

const ValorJson* ValorJson::buscar_ruta(const std::string& ruta) const {
    const ValorJson* actual = this;
    std::size_t inicio = 0;
    while (actual != nullptr && inicio <= ruta.size()) {
        std::size_t punto = ruta.find('.', inicio);
        std::string clave =
            ruta.substr(inicio, punto == std::string::npos ? std::string::npos : punto - inicio);
        actual = actual->buscar(clave);
        if (punto == std::string::npos) break;
        inicio = punto + 1;
    }
    return actual;
}

namespace {

class Lector {
public:
    explicit Lector(const std::string& t) : t_(t) {}

    ValorJson documento() {
        ValorJson v = valor(0);
        espacios();
        if (p_ != t_.size()) fallar("contenido extra al final");
        return v;
    }

private:
    const std::string& t_;
    std::size_t p_ = 0;

    [[noreturn]] void fallar(const std::string& msg) const {
        throw std::runtime_error("JSON inválido en la posición " + std::to_string(p_) + ": " + msg);
    }

    void espacios() {
        while (p_ < t_.size() &&
               (t_[p_] == ' ' || t_[p_] == '\n' || t_[p_] == '\r' || t_[p_] == '\t'))
            ++p_;
    }

    bool consumir(const char* literal) {
        std::size_t i = 0;
        while (literal[i] != '\0') {
            if (p_ + i >= t_.size() || t_[p_ + i] != literal[i]) return false;
            ++i;
        }
        p_ += i;
        return true;
    }

    ValorJson valor(int profundidad) {
        if (profundidad > 64) fallar("anidamiento excesivo");
        espacios();
        if (p_ >= t_.size()) fallar("fin inesperado");
        ValorJson v;
        char c = t_[p_];
        if (c == '{') {
            v.tipo = ValorJson::Tipo::Objeto;
            ++p_;
            espacios();
            if (p_ < t_.size() && t_[p_] == '}') {
                ++p_;
                return v;
            }
            for (;;) {
                espacios();
                if (p_ >= t_.size() || t_[p_] != '"') fallar("se esperaba una clave");
                std::string clave = cadena();
                espacios();
                if (p_ >= t_.size() || t_[p_] != ':') fallar("se esperaba ':'");
                ++p_;
                v.objeto.emplace_back(clave, valor(profundidad + 1));
                espacios();
                if (p_ < t_.size() && t_[p_] == ',') {
                    ++p_;
                    continue;
                }
                if (p_ < t_.size() && t_[p_] == '}') {
                    ++p_;
                    return v;
                }
                fallar("se esperaba ',' o '}'");
            }
        }
        if (c == '[') {
            v.tipo = ValorJson::Tipo::Arreglo;
            ++p_;
            espacios();
            if (p_ < t_.size() && t_[p_] == ']') {
                ++p_;
                return v;
            }
            for (;;) {
                v.arreglo.push_back(valor(profundidad + 1));
                espacios();
                if (p_ < t_.size() && t_[p_] == ',') {
                    ++p_;
                    continue;
                }
                if (p_ < t_.size() && t_[p_] == ']') {
                    ++p_;
                    return v;
                }
                fallar("se esperaba ',' o ']'");
            }
        }
        if (c == '"') {
            v.tipo = ValorJson::Tipo::Texto;
            v.texto = cadena();
            return v;
        }
        if (consumir("true")) {
            v.tipo = ValorJson::Tipo::Booleano;
            v.booleano = true;
            return v;
        }
        if (consumir("false")) {
            v.tipo = ValorJson::Tipo::Booleano;
            return v;
        }
        if (consumir("null")) return v;
        if (c == '-' || (c >= '0' && c <= '9')) return numero();
        fallar("valor no reconocido");
    }

    ValorJson numero() {
        std::size_t inicio = p_;
        if (t_[p_] == '-') ++p_;
        auto digitos = [&] {
            std::size_t d = p_;
            while (p_ < t_.size() && t_[p_] >= '0' && t_[p_] <= '9') ++p_;
            return p_ > d;
        };
        if (!digitos()) fallar("número inválido");
        if (p_ < t_.size() && t_[p_] == '.') {
            ++p_;
            if (!digitos()) fallar("número inválido");
        }
        if (p_ < t_.size() && (t_[p_] == 'e' || t_[p_] == 'E')) {
            ++p_;
            if (p_ < t_.size() && (t_[p_] == '+' || t_[p_] == '-')) ++p_;
            if (!digitos()) fallar("número inválido");
        }
        ValorJson v;
        v.tipo = ValorJson::Tipo::Numero;
        v.numero = std::strtod(t_.substr(inicio, p_ - inicio).c_str(), nullptr);
        return v;
    }

    static void anexar_utf8(std::string& s, unsigned long cp) {
        if (cp < 0x80) {
            s += static_cast<char>(cp);
        } else if (cp < 0x800) {
            s += static_cast<char>(0xC0 | (cp >> 6));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s += static_cast<char>(0xE0 | (cp >> 12));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            s += static_cast<char>(0xF0 | (cp >> 18));
            s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    unsigned long hex4() {
        if (p_ + 4 > t_.size()) fallar("escape \\u incompleto");
        unsigned long v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = t_[p_++];
            v <<= 4;
            if (c >= '0' && c <= '9')
                v |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f')
                v |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                v |= static_cast<unsigned long>(c - 'A' + 10);
            else
                fallar("dígito hexadecimal inválido");
        }
        return v;
    }

    std::string cadena() {
        ++p_;  // comilla inicial
        std::string s;
        while (p_ < t_.size()) {
            char c = t_[p_++];
            if (c == '"') return s;
            if (static_cast<unsigned char>(c) < 0x20) fallar("carácter de control en texto");
            if (c != '\\') {
                s += c;
                continue;
            }
            if (p_ >= t_.size()) break;
            char e = t_[p_++];
            switch (e) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case 'u': {
                    unsigned long cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (!consumir("\\u")) fallar("par sustituto incompleto");
                        unsigned long bajo = hex4();
                        if (bajo < 0xDC00 || bajo > 0xDFFF) fallar("par sustituto inválido");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (bajo - 0xDC00);
                    }
                    anexar_utf8(s, cp);
                    break;
                }
                default: fallar("escape desconocido");
            }
        }
        fallar("texto sin cerrar");
    }
};

}  // namespace

ValorJson parsear_json(const std::string& texto) {
    return Lector(texto).documento();
}

}  // namespace jarvis
