// Limpieza y saneamiento de texto entre etapas.
// El texto del usuario NUNCA se interpola en un shell; aun así se sanea
// para que no pueda confundirse con una opción de línea de comandos.
#pragma once

#include <cstddef>
#include <string>

namespace jarvis {

// Quita caracteres de control, colapsa espacios, recorta, elimina guiones
// iniciales (para que no parezca una opción) y limita a max_bytes sin cortar UTF-8.
std::string sanear_texto(const std::string& entrada, std::size_t max_bytes);

// Limpia la salida de whisper: quita marcas como [BLANK_AUDIO] o (música).
std::string limpiar_transcripcion(const std::string& salida_whisper);

// Neutraliza marcas especiales de ChatML ("<|im_start|>", "<think>", ...) para
// que el texto del usuario no pueda alterar la estructura del prompt.
std::string quitar_marcas_especiales(const std::string& texto);

// Limpia la salida de llama.cpp: quita bloques <think>…</think>, marcas de
// fin de texto, indicadores "> " del modo conversación y espacios sobrantes.
std::string limpiar_respuesta_slm(const std::string& salida_llama);

}  // namespace jarvis
