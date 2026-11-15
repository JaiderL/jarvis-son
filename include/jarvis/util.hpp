// Utilidades comunes: directorio temporal con limpieza automática,
// lectura/escritura de archivos y formato de fecha.
#pragma once

#include <string>

namespace jarvis {

// Crea un directorio temporal único (mkdtemp) en $TMPDIR o /tmp y lo borra
// con todo su contenido al destruirse, tanto si el turno termina bien como
// si falla. Así nunca quedan grabaciones ni textos en disco.
class DirTemporal {
public:
    explicit DirTemporal(const std::string& prefijo = "jarvis");
    ~DirTemporal();
    DirTemporal(const DirTemporal&) = delete;
    DirTemporal& operator=(const DirTemporal&) = delete;

    bool valido() const { return !ruta_.empty(); }
    const std::string& ruta() const { return ruta_; }
    std::string archivo(const std::string& nombre) const { return ruta_ + "/" + nombre; }
    void limpiar();

private:
    std::string ruta_;
};

bool existe_archivo(const std::string& ruta);
bool existe_directorio(const std::string& ruta);
long long tamano_archivo(const std::string& ruta);  // -1 si no existe
bool leer_archivo(const std::string& ruta, std::string& contenido);
bool escribir_archivo(const std::string& ruta, const std::string& contenido);
void borrar_archivo(const std::string& ruta);

// Fecha y hora local en formato ISO 8601 (sin zona), p. ej. 2026-11-15T10:20:30.
std::string ahora_iso();

// Últimas n líneas no vacías de un texto (para mostrar stderr en diagnósticos).
std::string ultimas_lineas(const std::string& texto, int n);

// Pausa en milisegundos que respeta la interrupción por Ctrl+C.
void dormir_ms(long ms);

}  // namespace jarvis
