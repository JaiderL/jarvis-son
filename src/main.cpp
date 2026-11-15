// Jarvis-son V1: asistente de voz local por turnos para Android (Termux).
// Flujo: MIC → STT (whisper.cpp) → SLM (llama.cpp) → TTS (sherpa-onnx/Piper).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "jarvis/atajos.hpp"
#include "jarvis/captura.hpp"
#include "jarvis/config.hpp"
#include "jarvis/orquestador.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/slm.hpp"
#include "jarvis/stt.hpp"
#include "jarvis/tts.hpp"

namespace {

constexpr const char* kVersion = "1.0.0";

void mostrar_ayuda() {
    std::cout <<
        R"(Uso: jarvis [opciones]

Opciones:
  --config RUTA     Archivo de configuración (por defecto: config/config.json)
  --perfil NOMBRE   Perfil a usar: base | alto (por defecto: perfil_activo)
  --turnos N        Número de turnos (0 = interactivo sin límite; por defecto 0)
  --auto            No espera Enter entre turnos (pruebas de aceptación)
  --wav RUTA        Usa este audio en lugar del micrófono
  --prueba ETAPA    Prueba una sola etapa: captura | stt | atajo | slm | tts
  --texto TEXTO     Texto de entrada para --prueba atajo, slm o tts
  --sin-metricas    No escribe logs/metricas.csv
  --version         Muestra la versión
  --ayuda           Muestra esta ayuda
)";
}

bool es_entero(const std::string& s) {
    if (s.empty() || s.size() > 6) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

int prueba_etapa(const jarvis::Config& c, const std::string& etapa, const std::string& wav,
                 const std::string& texto) {
    using namespace jarvis;
    DirTemporal dir("jarvis-prueba");
    if (etapa == "captura" || etapa == "stt") {
        ResultadoCaptura cap = capturar_audio(c, dir, wav);
        if (!cap.ok) {
            std::cerr << formatear_fallo(cap.fallo) << "\n";
            return 1;
        }
        std::printf("CAPTURA_OK duracion_s=%.2f rms=%.1f ms_grabacion=%ld ms_conversion=%ld\n",
                    cap.duracion_s, cap.rms, cap.ms_grabacion, cap.ms_conversion);
        if (etapa == "captura") return 0;
        ResultadoStt stt = transcribir(c, dir, cap.wav, cap.duracion_s);
        if (!stt.ok) {
            std::cerr << formatear_fallo(stt.fallo) << "\n";
            return 1;
        }
        std::printf("STT_OK ms=%ld rtf=%.3f rss_mb=%.0f\n", stt.ms, stt.rtf, stt.rss_kb / 1024.0);
        std::cout << "TRANSCRIPCION: " << stt.texto << "\n";
        return 0;
    }
    if (texto.empty()) {
        std::cerr << "Falta --texto para --prueba " << etapa << "\n";
        return 2;
    }
    if (etapa == "atajo") {
        // Muestra si la frase tendría respuesta rápida o iría al SLM.
        ContextoAtajo ctx;
        ctx.ahora = std::time(nullptr);
        if (std::optional<Atajo> a = buscar_atajo(c.atajos, texto, ctx)) {
            std::cout << "ATAJO id=" << a->id << (a->terminar ? " (termina la sesión)" : "")
                      << "\nRESPUESTA: " << a->respuesta << "\n";
        } else {
            std::cout << "SIN_ATAJO: la frase \"" << normalizar_frase(texto) << "\" iría al SLM\n";
        }
        return 0;
    }
    if (etapa == "slm") {
        ResultadoSlm slm = generar_respuesta(c, dir, texto);
        if (!slm.ok) {
            std::cerr << formatear_fallo(slm.fallo) << "\n";
            return 1;
        }
        std::printf("SLM_OK ms=%ld rss_mb=%.0f\n", slm.ms, slm.rss_kb / 1024.0);
        std::cout << "RESPUESTA: " << slm.texto << "\n";
        return 0;
    }
    if (etapa == "tts") {
        ResultadoTts tts = sintetizar_y_reproducir(c, dir, texto);
        if (!tts.ok) {
            std::cerr << formatear_fallo(tts.fallo) << "\n";
            return 1;
        }
        std::printf("TTS_OK motor=%s ms_sintesis=%ld ms_reproduccion=%ld rss_mb=%.0f\n",
                    tts.motor.c_str(), tts.ms_sintesis, tts.ms_reproduccion, tts.rss_kb / 1024.0);
        return 0;
    }
    std::cerr << "Etapa desconocida: " << etapa << " (use captura | stt | atajo | slm | tts)\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string ruta_config = "config/config.json";
    std::string perfil;
    std::string wav;
    std::string etapa;
    std::string texto;
    int turnos = 0;
    bool automatico = false;
    bool metricas = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto valor = [&](const char* nombre) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Falta el valor de " << nombre << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--config")
            ruta_config = valor("--config");
        else if (a == "--perfil")
            perfil = valor("--perfil");
        else if (a == "--wav")
            wav = valor("--wav");
        else if (a == "--prueba")
            etapa = valor("--prueba");
        else if (a == "--texto")
            texto = valor("--texto");
        else if (a == "--turnos") {
            std::string n = valor("--turnos");
            if (!es_entero(n)) {
                std::cerr << "--turnos debe ser un entero >= 0\n";
                return 2;
            }
            turnos = std::stoi(n);
        } else if (a == "--auto")
            automatico = true;
        else if (a == "--sin-metricas")
            metricas = false;
        else if (a == "--version") {
            std::cout << "jarvis-son " << kVersion << "\n";
            return 0;
        } else if (a == "--ayuda" || a == "-h" || a == "--help") {
            mostrar_ayuda();
            return 0;
        } else {
            std::cerr << "Opción desconocida: " << a << " (use --ayuda)\n";
            return 2;
        }
    }

    jarvis::Config config;
    try {
        config = jarvis::cargar_config(ruta_config, perfil);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Configuración: " << e.what() << "\n";
        return 3;
    }
    jarvis::instalar_manejador_senales();

    if (!etapa.empty()) return prueba_etapa(config, etapa, wav, texto);

    std::cout << "Jarvis-son " << kVersion << " | perfil: " << config.perfil.nombre
              << " | hilos: " << config.perfil.hilos << " | contexto: " << config.perfil.contexto
              << "\n";
    jarvis::Orquestador orquestador(config, metricas);
    int exitos = 0;
    int fallas = 0;
    for (int n = 1; turnos == 0 || n <= turnos; ++n) {
        if (jarvis::g_interrumpido) break;
        if (!automatico) {
            std::cout << "\n[LISTO] Presione Enter para hablar (q + Enter para salir): "
                      << std::flush;
            std::string linea;
            if (!std::getline(std::cin, linea) || linea == "q" || linea == "Q") break;
        } else {
            std::cout << "\n=== Turno " << n << (turnos > 0 ? "/" + std::to_string(turnos) : "")
                      << " ===\n";
        }
        if (orquestador.ejecutar_turno(n, wav))
            ++exitos;
        else
            ++fallas;
        // "Adiós" termina la sesión, salvo en pruebas con un número fijo de turnos.
        if (orquestador.salir_solicitado() && !(automatico && turnos > 0)) {
            std::cout << "Sesión terminada por el usuario.\n";
            break;
        }
    }
    std::cout << "\nTurnos exitosos: " << exitos << " | con error: " << fallas << "\n";
    return fallas == 0 ? 0 : 1;
}
