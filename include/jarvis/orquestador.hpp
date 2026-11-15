// Orquestador por turnos: LISTO → GRABANDO → STT → SLM → TTS → LISTO (o ERROR).
// Cada etapa termina (y su proceso hijo sale de memoria) antes de iniciar la
// siguiente: nunca hay dos modelos cargados a la vez.
#pragma once

#include <cstddef>
#include <string>

#include "jarvis/config.hpp"
#include "jarvis/estados.hpp"
#include "jarvis/metricas.hpp"

namespace jarvis {

class Orquestador {
public:
    explicit Orquestador(Config c, bool guardar_metricas = true);

    // Ejecuta un turno completo. wav_entrada vacío = micrófono.
    // Devuelve true si terminó en LISTO sin errores.
    bool ejecutar_turno(int numero, const std::string& wav_entrada);

    Estado estado() const { return estado_; }
    const MetricasTurno& ultimas_metricas() const { return metricas_; }
    const Config& config() const { return config_; }
    // true si una respuesta rápida pidió terminar la sesión (p. ej. "adiós").
    bool salir_solicitado() const { return salir_solicitado_; }

private:
    void cambiar_a(Estado nuevo);
    bool fallar(const FalloEtapa& f);

    Config config_;
    bool guardar_metricas_;
    Estado estado_ = Estado::Listo;
    MetricasTurno metricas_;
    std::string ultima_respuesta_;  // solo en memoria; nunca se guarda en disco
    std::size_t rotacion_ = 0;      // rota entre respuestas rápidas alternativas
    bool salir_solicitado_ = false;
};

}  // namespace jarvis
