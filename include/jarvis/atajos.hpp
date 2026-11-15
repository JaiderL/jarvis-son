// Respuestas rápidas: frases cortas y frecuentes ("hola", "¿qué hora es?")
// que se responden en C++ sin cargar el SLM. Ahorra RAM (~1 GB) y segundos
// por turno. Las reglas viven en config/config.json ("respuestas_rapidas").
#pragma once

#include <cstddef>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace jarvis {

struct ReglaAtajo {
    std::string id;
    std::string modo;                     // "exacta" o "palabras_clave"
    std::vector<std::string> frases;      // modo exacta (ya normalizadas)
    std::vector<std::string> todas;       // modo palabras_clave: deben estar todas
    std::vector<std::string> alguna;      // modo palabras_clave: al menos una (opcional)
    std::vector<std::string> respuestas;  // se rotan entre turnos
    std::string dinamica;                 // "", "hora", "fecha" o "ultima_respuesta"
    bool terminar = false;                // la sesión termina tras responder
};

struct ConfigAtajos {
    bool activado = false;
    int max_palabras = 6;            // solo se buscan atajos en frases cortas
    bool tolerancia_errores = true;  // acepta 1 error por palabra (p. ej. "ola" ≈ "hola")
    std::vector<ReglaAtajo> reglas;
    std::vector<std::string> temas_bloqueados;  // frases normalizadas
    std::string respuesta_bloqueo;
};

// Datos del momento que necesitan las respuestas dinámicas.
struct ContextoAtajo {
    std::time_t ahora = 0;
    std::string ultima_respuesta;  // solo en memoria, nunca en disco
    std::size_t rotacion = 0;      // elige la respuesta entre varias
};

struct Atajo {
    std::string id;         // id de la regla, o "bloqueo"
    std::string respuesta;  // texto que se enviará al TTS
    bool terminar = false;
};

// Minúsculas, sin tildes (conserva la ñ), sin signos y con espacios simples.
// "¡Hola, ¿CÓMO estás?" → "hola como estas".
std::string normalizar_frase(const std::string& texto);

std::vector<std::string> separar_palabras(const std::string& normalizada);

// Distancia de Levenshtein (por bytes) entre dos palabras normalizadas.
int distancia_edicion(const std::string& a, const std::string& b);

// Devuelve la respuesta rápida si la transcripción coincide con una regla o
// con un tema bloqueado; std::nullopt si debe ir al SLM.
std::optional<Atajo> buscar_atajo(const ConfigAtajos& c, const std::string& transcripcion,
                                  const ContextoAtajo& ctx);

// Textos en español para las respuestas dinámicas.
std::string texto_hora(const std::tm& t);
std::string texto_fecha(const std::tm& t);

}  // namespace jarvis
