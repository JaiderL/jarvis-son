#include "jarvis/texto.hpp"

#include <vector>

namespace jarvis {

namespace {

void reemplazar_todo(std::string& s, const std::string& de, const std::string& a) {
    if (de.empty()) return;
    std::size_t p = 0;
    while ((p = s.find(de, p)) != std::string::npos) {
        s.replace(p, de.size(), a);
        p += a.size();
    }
}

// Elimina segmentos delimitados por abre/cierra (sin anidar).
std::string quitar_entre(const std::string& s, char abre, char cierra) {
    std::string r;
    int nivel = 0;
    for (char c : s) {
        if (c == abre) {
            ++nivel;
            continue;
        }
        if (c == cierra && nivel > 0) {
            --nivel;
            r += ' ';
            continue;
        }
        if (nivel == 0) r += c;
    }
    return r;
}

}  // namespace

std::string sanear_texto(const std::string& entrada, std::size_t max_bytes) {
    std::string r;
    r.reserve(entrada.size());
    bool espacio_previo = true;  // evita espacios al inicio
    for (unsigned char c : entrada) {
        bool es_espacio = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (es_espacio) {
            if (!espacio_previo) r += ' ';
            espacio_previo = true;
            continue;
        }
        if (c < 0x20 || c == 0x7F) continue;  // controles
        r += static_cast<char>(c);
        espacio_previo = false;
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    // Sin guiones ni espacios iniciales: el texto nunca debe parecer una opción.
    std::size_t inicio = r.find_first_not_of("- ");
    r = (inicio == std::string::npos) ? std::string() : r.substr(inicio);
    if (r.size() > max_bytes) {
        std::size_t corte = max_bytes;
        // Retrocede hasta un inicio de carácter UTF-8.
        while (corte > 0 && (static_cast<unsigned char>(r[corte]) & 0xC0) == 0x80) --corte;
        r.resize(corte);
        while (!r.empty() && r.back() == ' ') r.pop_back();
    }
    return r;
}

std::string limpiar_transcripcion(const std::string& salida_whisper) {
    std::string s = quitar_entre(salida_whisper, '[', ']');
    s = quitar_entre(s, '(', ')');
    return sanear_texto(s, 1000);
}

std::string quitar_marcas_especiales(const std::string& texto) {
    std::string s = texto;
    // Las marcas de control de ChatML empiezan con "<|"; también se neutralizan
    // las etiquetas de razonamiento para que el usuario no altere el prompt.
    reemplazar_todo(s, "<|", "< |");
    reemplazar_todo(s, "<think>", "think");
    reemplazar_todo(s, "</think>", "think");
    return s;
}

std::string limpiar_respuesta_slm(const std::string& salida_llama) {
    std::string s = salida_llama;
    // Bloques de razonamiento completos.
    for (;;) {
        std::size_t a = s.find("<think>");
        if (a == std::string::npos) break;
        std::size_t b = s.find("</think>", a);
        if (b == std::string::npos) {
            s.erase(a);  // razonamiento sin cerrar: se descarta el resto
            break;
        }
        s.erase(a, b + 8 - a);
    }
    // Un "</think>" sin apertura indica que el razonamiento empezó dentro del
    // prompt: todo lo anterior es razonamiento y se descarta.
    std::size_t cierre = s.rfind("</think>");
    if (cierre != std::string::npos) s.erase(0, cierre + 8);
    for (const char* marca : {"[end of text]", "<|im_end|>", "<|endoftext|>", "<|im_start|>",
                              "assistant\n", "EOF by user"}) {
        reemplazar_todo(s, marca, " ");
    }
    // Quita el indicador "> " que imprime el modo conversación.
    std::string r;
    std::size_t p = 0;
    while (p <= s.size()) {
        std::size_t fin = s.find('\n', p);
        std::string linea = s.substr(p, fin == std::string::npos ? std::string::npos : fin - p);
        std::size_t q = linea.find_first_not_of(' ');
        if (q != std::string::npos && linea[q] == '>') linea = linea.substr(q + 1);
        r += linea + "\n";
        if (fin == std::string::npos) break;
        p = fin + 1;
    }
    reemplazar_todo(r, "**", "");
    reemplazar_todo(r, "#", "");
    return sanear_texto(r, 1200);
}

}  // namespace jarvis
