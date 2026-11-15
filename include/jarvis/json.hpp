// Lector JSON mínimo (sin dependencias externas) para config/config.json.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace jarvis {

class ValorJson {
public:
    enum class Tipo { Nulo, Booleano, Numero, Texto, Arreglo, Objeto };

    Tipo tipo = Tipo::Nulo;
    bool booleano = false;
    double numero = 0.0;
    std::string texto;
    std::vector<ValorJson> arreglo;
    std::vector<std::pair<std::string, ValorJson>> objeto;

    // Busca una clave en un objeto; nullptr si no existe o no es objeto.
    const ValorJson* buscar(const std::string& clave) const;
    // Busca una ruta con puntos, p. ej. "grabacion.segundos".
    const ValorJson* buscar_ruta(const std::string& ruta) const;
};

// Lanza std::runtime_error con la posición del error si el JSON es inválido.
ValorJson parsear_json(const std::string& texto);

}  // namespace jarvis
