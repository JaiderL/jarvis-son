#include "jarvis/wav.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace jarvis {

namespace {

std::uint32_t u32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t u16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

void poner_u32(std::ofstream& f, std::uint32_t v) {
    unsigned char b[4] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8),
                          static_cast<unsigned char>(v >> 16), static_cast<unsigned char>(v >> 24)};
    f.write(reinterpret_cast<const char*>(b), 4);
}

void poner_u16(std::ofstream& f, std::uint16_t v) {
    unsigned char b[2] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8)};
    f.write(reinterpret_cast<const char*>(b), 2);
}

}  // namespace

bool leer_info_wav(const std::string& ruta, InfoWav& info, std::string& error) {
    info = InfoWav{};
    std::ifstream f(ruta, std::ios::binary);
    if (!f) {
        error = "no existe o no se puede abrir el WAV";
        return false;
    }
    f.seekg(0, std::ios::end);
    const long long tamano = static_cast<long long>(f.tellg());
    f.seekg(0, std::ios::beg);

    unsigned char cab[12];
    if (!f.read(reinterpret_cast<char*>(cab), 12) || std::memcmp(cab, "RIFF", 4) != 0 ||
        std::memcmp(cab + 8, "WAVE", 4) != 0) {
        error = "no es un archivo RIFF/WAVE";
        return false;
    }

    bool hay_fmt = false;
    bool hay_datos = false;
    long long inicio_datos = 0;
    while (!hay_datos) {
        unsigned char trozo[8];
        if (!f.read(reinterpret_cast<char*>(trozo), 8)) break;
        std::uint32_t largo = u32(trozo + 4);
        long long pos = static_cast<long long>(f.tellg());
        if (std::memcmp(trozo, "fmt ", 4) == 0) {
            unsigned char fmt[16];
            if (largo < 16 || !f.read(reinterpret_cast<char*>(fmt), 16)) {
                error = "bloque fmt incompleto";
                return false;
            }
            info.formato = u16(fmt);
            info.canales = u16(fmt + 2);
            info.frecuencia = static_cast<int>(u32(fmt + 4));
            info.bits = u16(fmt + 14);
            hay_fmt = true;
        } else if (std::memcmp(trozo, "data", 4) == 0) {
            hay_datos = true;
            inicio_datos = pos;
            long long disponibles = tamano - pos;
            if (disponibles < 0) disponibles = 0;
            // ffmpeg en modo flujo puede dejar 0 o 0xFFFFFFFF; se usa lo real.
            long long real = (largo == 0 || largo == 0xFFFFFFFFu || largo > disponibles)
                                 ? disponibles
                                 : static_cast<long long>(largo);
            info.bytes_datos = static_cast<std::uint32_t>(real);
            break;
        }
        f.seekg(pos + static_cast<long long>(largo) + (largo & 1u), std::ios::beg);
        if (!f) break;
    }
    if (!hay_fmt || !hay_datos) {
        error = "faltan los bloques fmt o data";
        return false;
    }
    const int bytes_por_muestra = info.bits / 8;
    if (info.frecuencia <= 0 || info.canales <= 0 || bytes_por_muestra <= 0) {
        error = "cabecera WAV con valores inválidos";
        return false;
    }
    info.duracion_s = static_cast<double>(info.bytes_datos) /
                      (static_cast<double>(info.frecuencia) * info.canales * bytes_por_muestra);

    if (info.formato == 1 && info.bits == 16 && info.bytes_datos >= 2) {
        f.clear();
        f.seekg(inicio_datos, std::ios::beg);
        double suma = 0.0;
        std::uint64_t n = 0;
        std::vector<char> bufer(1 << 15);
        std::uint32_t restantes = info.bytes_datos & ~1u;
        while (restantes > 0 && f) {
            std::uint32_t pedir =
                restantes < bufer.size() ? restantes : static_cast<std::uint32_t>(bufer.size());
            f.read(bufer.data(), pedir);
            std::streamsize leidos = f.gcount();
            if (leidos <= 0) break;
            for (std::streamsize i = 0; i + 1 < leidos; i += 2) {
                const auto* p = reinterpret_cast<const unsigned char*>(bufer.data() + i);
                auto m = static_cast<std::int16_t>(u16(p));
                suma += static_cast<double>(m) * m;
                ++n;
            }
            restantes -= static_cast<std::uint32_t>(leidos);
        }
        info.rms = n > 0 ? std::sqrt(suma / static_cast<double>(n)) : 0.0;
    }
    return true;
}

std::string validar_wav_stt(const InfoWav& info, double min_s, double max_s, double rms_minimo) {
    char bufer[160];
    if (info.formato != 1 || info.bits != 16) return "el audio no es PCM s16le";
    if (info.canales != 1)
        return "el audio no es mono (" + std::to_string(info.canales) + " canales)";
    if (info.frecuencia != 16000)
        return "la frecuencia es " + std::to_string(info.frecuencia) + " Hz (se espera 16000)";
    // Tolerancia de 50 ms por redondeo de ffmpeg.
    if (info.duracion_s + 0.05 < min_s || info.duracion_s - 0.05 > max_s) {
        std::snprintf(bufer, sizeof bufer, "duración %.2f s fuera del rango [%.1f, %.1f] s",
                      info.duracion_s, min_s, max_s);
        return bufer;
    }
    if (info.rms < rms_minimo) {
        std::snprintf(bufer, sizeof bufer, "grabación vacía o en silencio (RMS %.1f < %.1f)",
                      info.rms, rms_minimo);
        return bufer;
    }
    return "";
}

bool escribir_wav_s16(const std::string& ruta, const std::vector<std::int16_t>& muestras,
                      int frecuencia, int canales) {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const auto bytes = static_cast<std::uint32_t>(muestras.size() * 2);
    f.write("RIFF", 4);
    poner_u32(f, 36 + bytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    poner_u32(f, 16);
    poner_u16(f, 1);
    poner_u16(f, static_cast<std::uint16_t>(canales));
    poner_u32(f, static_cast<std::uint32_t>(frecuencia));
    poner_u32(f, static_cast<std::uint32_t>(frecuencia * canales * 2));
    poner_u16(f, static_cast<std::uint16_t>(canales * 2));
    poner_u16(f, 16);
    f.write("data", 4);
    poner_u32(f, bytes);
    for (std::int16_t m : muestras) poner_u16(f, static_cast<std::uint16_t>(m));
    return static_cast<bool>(f);
}

}  // namespace jarvis
