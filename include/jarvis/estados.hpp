// Máquina de estados del turno: LISTO → GRABANDO → STT → SLM → TTS → LISTO.
// Cualquier etapa puede pasar a ERROR, y ERROR vuelve a LISTO.
#pragma once

#include <string>

namespace jarvis {

enum class Estado { Listo, Grabando, Stt, Slm, Tts, Error };

const char* nombre_estado(Estado e);
bool transicion_valida(Estado desde, Estado hacia);

// Describe una falla de etapa para mostrarla al usuario.
struct FalloEtapa {
    Estado etapa = Estado::Error;
    int codigo = 0;              // código de salida del proceso (o interno)
    std::string detalle;         // qué pasó
    std::string siguiente_paso;  // diagnóstico sugerido
};

// Texto listo para imprimir: etapa, código, detalle y siguiente paso.
std::string formatear_fallo(const FalloEtapa& f);

}  // namespace jarvis
