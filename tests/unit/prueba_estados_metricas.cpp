// Pruebas de la máquina de estados y del formato de métricas.
#include "jarvis/estados.hpp"
#include "jarvis/metricas.hpp"
#include "jarvis/util.hpp"
#include "prueba.hpp"

using namespace jarvis;

PRUEBA(estados_flujo_feliz_es_valido) {
    VERIFICAR(transicion_valida(Estado::Listo, Estado::Grabando));
    VERIFICAR(transicion_valida(Estado::Grabando, Estado::Stt));
    VERIFICAR(transicion_valida(Estado::Stt, Estado::Slm));
    VERIFICAR(transicion_valida(Estado::Slm, Estado::Tts));
    VERIFICAR(transicion_valida(Estado::Tts, Estado::Listo));
}

PRUEBA(estados_no_permite_saltar_etapas) {
    VERIFICAR(!transicion_valida(Estado::Listo, Estado::Slm));
    VERIFICAR(!transicion_valida(Estado::Grabando, Estado::Tts));
    VERIFICAR(!transicion_valida(Estado::Stt, Estado::Listo));
    VERIFICAR(!transicion_valida(Estado::Tts, Estado::Grabando));
}

PRUEBA(estados_error_desde_cualquier_etapa_y_vuelve_a_listo) {
    for (Estado e : {Estado::Grabando, Estado::Stt, Estado::Slm, Estado::Tts})
        VERIFICAR(transicion_valida(e, Estado::Error));
    VERIFICAR(transicion_valida(Estado::Error, Estado::Listo));
    VERIFICAR(!transicion_valida(Estado::Error, Estado::Stt));
}

PRUEBA(estados_formato_de_fallo_incluye_etapa_codigo_y_paso) {
    FalloEtapa f;
    f.etapa = Estado::Stt;
    f.codigo = 124;
    f.detalle = "whisper-cli: superó el tiempo límite";
    f.siguiente_paso = "Use el perfil base";
    std::string s = formatear_fallo(f);
    VERIFICAR(s.find("STT") != std::string::npos);
    VERIFICAR(s.find("124") != std::string::npos);
    VERIFICAR(s.find("Siguiente paso") != std::string::npos);
}

PRUEBA(metricas_csv_tiene_columnas_esperadas_y_sin_texto) {
    std::string cab = encabezado_csv();
    for (const char* col : {"ms_stt", "ms_slm", "ms_tts", "ms_total", "rtf_stt", "rss_slm_kb"})
        VERIFICAR(cab.find(col) != std::string::npos);
    // Privacidad: no hay columnas de transcripción ni respuesta.
    VERIFICAR(cab.find("texto") == std::string::npos);
    VERIFICAR(cab.find("transcripcion") == std::string::npos);
    VERIFICAR(cab.find("respuesta") == std::string::npos);
}

PRUEBA(metricas_fila_tiene_mismo_numero_de_columnas) {
    MetricasTurno m;
    m.fecha = "2026-11-15T10:00:00";
    m.perfil = "base";
    m.etapa_error = "a,b\"c";  // se filtran caracteres que romperían el CSV
    auto columnas = [](const std::string& s) {
        std::size_t n = 1;
        for (char c : s) n += (c == ',');
        return n;
    };
    VERIFICAR_IGUAL(columnas(fila_csv(m)), columnas(encabezado_csv()));
}

PRUEBA(metricas_anexar_crea_encabezado_una_vez) {
    DirTemporal dir("jarvis-prueba");
    const std::string ruta = dir.archivo("logs/metricas.csv");
    MetricasTurno m;
    VERIFICAR(anexar_metricas(ruta, m));
    VERIFICAR(anexar_metricas(ruta, m));
    std::string contenido;
    VERIFICAR(leer_archivo(ruta, contenido));
    std::size_t lineas = 0;
    for (char c : contenido) lineas += (c == '\n');
    VERIFICAR_IGUAL(lineas, static_cast<std::size_t>(3));
}
