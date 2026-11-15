#!/usr/bin/env python3
"""Reporte de latencias (p50/p95) y memoria pico a partir de logs/metricas.csv.

Uso:
    python3 tests/reporte_latencias.py logs/metricas.csv
    python3 tests/reporte_latencias.py logs/metricas.csv --ultimos 10 --perfil base
    python3 tests/reporte_latencias.py --autoprueba
"""

import argparse
import csv
import io
import math
import sys

ETAPAS = [
    ("ms_grabacion", "Grabación"),
    ("ms_conversion", "Conversión FFmpeg"),
    ("ms_stt", "STT (whisper)"),
    ("ms_slm", "SLM (llama)"),
    ("ms_tts", "TTS síntesis"),
    ("ms_reproduccion", "Reproducción"),
    ("ms_total", "Total del turno"),
]
MEMORIA = [
    ("rss_ffmpeg_kb", "ffmpeg"),
    ("rss_stt_kb", "whisper-cli"),
    ("rss_slm_kb", "llama-completion"),
    ("rss_tts_kb", "sherpa-onnx"),
]


def percentil(valores: list[float], p: float) -> float:
    """Percentil con interpolación lineal (igual que numpy por defecto)."""
    if not valores:
        return float("nan")
    datos = sorted(valores)
    k = (len(datos) - 1) * p / 100.0
    piso = math.floor(k)
    techo = math.ceil(k)
    if piso == techo:
        return datos[int(k)]
    return datos[piso] + (datos[techo] - datos[piso]) * (k - piso)


def leer_filas(flujo) -> list[dict]:
    return list(csv.DictReader(flujo))


def reportar(filas: list[dict], ultimos: int | None, perfil: str | None) -> dict:
    if perfil:
        filas = [f for f in filas if f.get("perfil") == perfil]
    if ultimos:
        filas = filas[-ultimos:]
    exitosas = [f for f in filas if f.get("estado") == "ok"]
    errores = [f for f in filas if f.get("estado") != "ok"]

    print(f"Turnos analizados: {len(filas)} | exitosos: {len(exitosas)} | con error: {len(errores)}")
    if errores:
        conteo: dict[str, int] = {}
        for f in errores:
            clave = f"{f.get('etapa_error', '?')} (código {f.get('codigo_error', '?')})"
            conteo[clave] = conteo.get(clave, 0) + 1
        for clave, n in sorted(conteo.items()):
            print(f"  Error en {clave}: {n}")
    resultado: dict = {"turnos": len(filas), "exitosos": len(exitosas)}
    if not exitosas:
        print("No hay turnos exitosos para calcular percentiles.")
        return resultado

    # Ruta de cada turno: "slm" (pasó por el modelo) o "atajo"/"bloqueo" (respuesta rápida).
    rutas: dict[str, list[dict]] = {}
    for f in exitosas:
        rutas.setdefault(f.get("ruta") or "slm", []).append(f)
    resultado["rutas"] = {r: len(v) for r, v in rutas.items()}
    if len(rutas) > 1 or "slm" not in rutas:
        partes = ", ".join(f"{r}: {len(v)}" for r, v in sorted(rutas.items()))
        print(f"Turnos exitosos por ruta: {partes}")

    print()
    print(f"{'Etapa':<20} {'p50 (s)':>9} {'p95 (s)':>9} {'máx (s)':>9}")
    for columna, nombre in ETAPAS:
        # El SLM solo se mide en los turnos que lo usaron.
        base = rutas.get("slm", []) if columna == "ms_slm" else exitosas
        valores = [float(f[columna]) / 1000.0 for f in base if f.get(columna, "") != ""]
        if not valores:
            print(f"{nombre:<20} {'-':>9} {'-':>9} {'-':>9}")
            continue
        p50, p95 = percentil(valores, 50), percentil(valores, 95)
        resultado[columna] = (p50, p95)
        print(f"{nombre:<20} {p50:9.2f} {p95:9.2f} {max(valores):9.2f}")

    rtf = [float(f["rtf_stt"]) for f in exitosas if f.get("rtf_stt")]
    if rtf:
        print(f"{'RTF STT':<20} {percentil(rtf, 50):9.2f} {percentil(rtf, 95):9.2f} {max(rtf):9.2f}")

    if len(rutas) > 1:
        print()
        print(f"{'Total por ruta':<20} {'p50 (s)':>9} {'p95 (s)':>9}")
        for ruta, filas_ruta in sorted(rutas.items()):
            valores = [float(f["ms_total"]) / 1000.0 for f in filas_ruta]
            print(f"{ruta:<20} {percentil(valores, 50):9.2f} {percentil(valores, 95):9.2f}")

    print()
    print(f"{'Proceso':<20} {'memoria pico máx (MB)':>22}")
    for columna, nombre in MEMORIA:
        valores = [float(f[columna]) / 1024.0 for f in filas if f.get(columna, "") not in ("", "0")]
        maximo = max(valores) if valores else 0.0
        resultado[columna] = maximo
        print(f"{nombre:<20} {maximo:22.0f}")
    return resultado


def autoprueba() -> int:
    assert percentil([1, 2, 3, 4, 5], 50) == 3
    assert abs(percentil([1, 2, 3, 4, 5], 95) - 4.8) < 1e-9
    assert percentil([7], 95) == 7
    csv_ejemplo = io.StringIO(
        "fecha,perfil,turno,estado,etapa_error,codigo_error,ms_grabacion,ms_conversion,"
        "ms_stt,ms_slm,ms_tts,ms_reproduccion,ms_total,duracion_audio_s,rtf_stt,"
        "rss_ffmpeg_kb,rss_stt_kb,rss_slm_kb,rss_tts_kb,motor_tts,ruta\n"
        "x,base,1,ok,,0,6000,100,2000,8000,1500,3000,20600,6.00,0.333,50000,150000,700000,120000,sherpa,slm\n"
        "x,base,2,ok,,0,6000,120,3000,9000,1600,3000,22720,6.00,0.500,51200,160000,720000,121000,sherpa,slm\n"
        "x,base,3,error,SLM,124,6000,110,2500,0,0,0,8610,6.00,0.417,50000,155000,0,0,,slm\n"
        "x,base,4,ok,,0,6000,100,2000,0,1000,2000,11100,6.00,0.333,50000,150000,0,120000,sherpa,atajo\n"
    )
    r = reportar(leer_filas(csv_ejemplo), None, "base")
    assert r["turnos"] == 4 and r["exitosos"] == 3
    assert r["rutas"] == {"slm": 2, "atajo": 1}
    assert abs(r["ms_slm"][0] - 8.5) < 1e-9  # el atajo (0 ms) no cuenta para el SLM
    assert abs(r["ms_stt"][0] - 2.0) < 1e-9
    assert abs(r["rss_slm_kb"] - 720000 / 1024) < 1e-9
    print("Autoprueba de reporte_latencias.py: OK")
    return 0


def main() -> int:
    analizador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    analizador.add_argument("csv", nargs="?", default="logs/metricas.csv")
    analizador.add_argument("--ultimos", type=int, default=None, help="analizar solo los últimos N turnos")
    analizador.add_argument("--perfil", default=None, help="filtrar por perfil (base | alto)")
    analizador.add_argument("--autoprueba", action="store_true")
    args = analizador.parse_args()
    if args.autoprueba:
        return autoprueba()
    try:
        with open(args.csv, encoding="utf-8", newline="") as f:
            filas = leer_filas(f)
    except FileNotFoundError:
        print(f"No existe {args.csv}: ejecute primero el orquestador.", file=sys.stderr)
        return 1
    reportar(filas, args.ultimos, args.perfil)
    return 0


if __name__ == "__main__":
    sys.exit(main())
