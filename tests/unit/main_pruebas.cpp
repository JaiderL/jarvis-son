// Ejecuta todas las pruebas registradas con PRUEBA(...).
#include "prueba.hpp"

int main() {
    int fallidas = 0;
    for (const auto& c : prueba::casos()) {
        int antes = prueba::fallas();
        c.funcion();
        bool ok = prueba::fallas() == antes;
        if (!ok) ++fallidas;
        std::cout << (ok ? "[OK]    " : "[FALLA] ") << c.nombre << "\n";
    }
    std::cout << "\n" << prueba::casos().size() << " pruebas, " << fallidas << " con fallas\n";
    return fallidas == 0 ? 0 : 1;
}
