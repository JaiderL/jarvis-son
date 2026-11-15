// Mini marco de pruebas sin dependencias (para compilar igual en Termux y CI).
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace prueba {

struct Caso {
    const char* nombre;
    std::function<void()> funcion;
};

inline std::vector<Caso>& casos() {
    static std::vector<Caso> lista;
    return lista;
}

inline int& fallas() {
    static int n = 0;
    return n;
}

struct Registro {
    Registro(const char* nombre, std::function<void()> f) { casos().push_back({nombre, f}); }
};

}  // namespace prueba

#define PRUEBA(nombre)                                            \
    static void nombre();                                         \
    static ::prueba::Registro registro_##nombre(#nombre, nombre); \
    static void nombre()

#define VERIFICAR(cond)                                                                \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::cerr << "  FALLO " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
            ++::prueba::fallas();                                                      \
        }                                                                              \
    } while (0)

#define VERIFICAR_IGUAL(a, b)                                                                \
    do {                                                                                     \
        auto va_ = (a);                                                                      \
        auto vb_ = (b);                                                                      \
        if (!(va_ == vb_)) {                                                                 \
            std::cerr << "  FALLO " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b      \
                      << "\n    obtenido: [" << va_ << "]\n    esperado: [" << vb_ << "]\n"; \
            ++::prueba::fallas();                                                            \
        }                                                                                    \
    } while (0)
