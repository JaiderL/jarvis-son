#include "jarvis/util.hpp"

#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "jarvis/proceso.hpp"

namespace fs = std::filesystem;

namespace jarvis {

DirTemporal::DirTemporal(const std::string& prefijo) {
    const char* tmp = std::getenv("TMPDIR");
    std::string base = (tmp != nullptr && *tmp != '\0') ? tmp : "/tmp";
    std::string plantilla = base + "/" + prefijo + "-XXXXXX";
    std::vector<char> bufer(plantilla.begin(), plantilla.end());
    bufer.push_back('\0');
    if (mkdtemp(bufer.data()) != nullptr) ruta_ = bufer.data();
}

DirTemporal::~DirTemporal() {
    limpiar();
}

void DirTemporal::limpiar() {
    if (ruta_.empty()) return;
    std::error_code ec;
    fs::remove_all(ruta_, ec);
    ruta_.clear();
}

bool existe_archivo(const std::string& ruta) {
    std::error_code ec;
    return fs::is_regular_file(ruta, ec);
}

bool existe_directorio(const std::string& ruta) {
    std::error_code ec;
    return fs::is_directory(ruta, ec);
}

long long tamano_archivo(const std::string& ruta) {
    std::error_code ec;
    auto t = fs::file_size(ruta, ec);
    return ec ? -1 : static_cast<long long>(t);
}

bool leer_archivo(const std::string& ruta, std::string& contenido) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return false;
    std::ostringstream os;
    os << f.rdbuf();
    contenido = os.str();
    return true;
}

bool escribir_archivo(const std::string& ruta, const std::string& contenido) {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << contenido;
    return static_cast<bool>(f);
}

void borrar_archivo(const std::string& ruta) {
    std::error_code ec;
    fs::remove(ruta, ec);
}

std::string ahora_iso() {
    std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_r(&t, &local);
    char bufer[32];
    std::strftime(bufer, sizeof bufer, "%Y-%m-%dT%H:%M:%S", &local);
    return bufer;
}

std::string ultimas_lineas(const std::string& texto, int n) {
    std::vector<std::string> lineas;
    std::istringstream is(texto);
    std::string linea;
    while (std::getline(is, linea)) {
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.find_first_not_of(" \t") != std::string::npos) lineas.push_back(linea);
    }
    std::string salida;
    std::size_t desde = lineas.size() > static_cast<std::size_t>(n) ? lineas.size() - n : 0;
    for (std::size_t i = desde; i < lineas.size(); ++i) {
        salida += "    " + lineas[i];
        if (i + 1 < lineas.size()) salida += "\n";
    }
    return salida;
}

void dormir_ms(long ms) {
    while (ms > 0 && !g_interrumpido) {
        long paso = ms < 100 ? ms : 100;
        usleep(static_cast<useconds_t>(paso * 1000));
        ms -= paso;
    }
}

}  // namespace jarvis
