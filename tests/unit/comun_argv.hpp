// Utilidades compartidas por las pruebas de argv de cada etapa.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace prueba {

inline const std::string& ruta_config() {
    static const std::string r = std::string(JARVIS_DIR_FUENTE) + "/config/config.json";
    return r;
}

inline bool contiene(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

inline std::string siguiente(const std::vector<std::string>& v, const std::string& opcion) {
    auto it = std::find(v.begin(), v.end(), opcion);
    return (it != v.end() && it + 1 != v.end()) ? *(it + 1) : "";
}

}  // namespace prueba
