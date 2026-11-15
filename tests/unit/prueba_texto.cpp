// Pruebas de limpieza y saneamiento de texto.
#include "jarvis/texto.hpp"
#include "prueba.hpp"

using namespace jarvis;

PRUEBA(texto_sanear_quita_controles_y_espacios) {
    VERIFICAR_IGUAL(sanear_texto("  hola\t\n mundo \x01\x7f ", 100), std::string("hola mundo"));
}

PRUEBA(texto_sanear_quita_guiones_iniciales) {
    // Evita que el texto se interprete como una opción del programa.
    VERIFICAR_IGUAL(sanear_texto("--help me", 100), std::string("help me"));
    VERIFICAR_IGUAL(sanear_texto(" - - -", 100), std::string(""));
}

PRUEBA(texto_sanear_no_corta_utf8) {
    // Cada ñ ocupa 2 bytes: con 4 bytes no cabe la segunda ñ completa.
    VERIFICAR_IGUAL(sanear_texto("ñañañaña", 4), std::string("ña"));
    VERIFICAR_IGUAL(sanear_texto("ñañañaña", 5), std::string("ñañ"));
}

PRUEBA(texto_limpia_transcripcion_de_whisper) {
    VERIFICAR_IGUAL(limpiar_transcripcion(" [BLANK_AUDIO]\n Hola, ¿qué hora es? (música)\n"),
                    std::string("Hola, ¿qué hora es?"));
    VERIFICAR_IGUAL(limpiar_transcripcion("[BLANK_AUDIO]"), std::string(""));
}

PRUEBA(texto_limpia_respuesta_con_think) {
    VERIFICAR_IGUAL(limpiar_respuesta_slm("<think>\n\n</think>\n\nHola, soy Jarvis.\n"),
                    std::string("Hola, soy Jarvis."));
    VERIFICAR_IGUAL(limpiar_respuesta_slm("<think>razonando sin cerrar..."), std::string(""));
}

PRUEBA(texto_limpia_marcas_de_llama) {
    VERIFICAR_IGUAL(limpiar_respuesta_slm("> Son las tres. [end of text]\n> "),
                    std::string("Son las tres."));
    VERIFICAR_IGUAL(limpiar_respuesta_slm("**Hola** <|im_end|>"), std::string("Hola"));
}

PRUEBA(texto_descarta_razonamiento_sin_apertura) {
    // Si el prompt ya abrió <think>, la salida solo trae el cierre.
    VERIFICAR_IGUAL(limpiar_respuesta_slm("El usuario pregunta... debo responder.\n</think>\n\n"
                                          "La capital es Bogotá. [end of text]"),
                    std::string("La capital es Bogotá."));
}
