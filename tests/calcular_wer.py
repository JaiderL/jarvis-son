#!/usr/bin/env python3
"""Calcula el WER (Word Error Rate) de las transcripciones de whisper.

Compara línea por línea las frases de referencia con las hipótesis
(transcripciones) y reporta el WER de cada frase y el global.

Una transcripción se considera "utilizable" si su WER es menor o igual al
umbral (por defecto 0.30). Meta del proyecto: al menos 18 de 20 utilizables.

Uso:
    python3 tests/calcular_wer.py tests/frases_referencia.txt logs/hipotesis_stt.txt
    python3 tests/calcular_wer.py --umbral 0.25 --meta 18 REFERENCIAS HIPOTESIS
    python3 tests/calcular_wer.py --autoprueba
"""

import argparse
import re
import sys
import unicodedata


def normalizar(texto: str) -> list[str]:
    """Minúsculas, sin tildes ni signos de puntuación; devuelve las palabras."""
    texto = texto.lower().strip()
    # Quita tildes pero conserva la ñ.
    texto = texto.replace("ñ", "\0")
    texto = unicodedata.normalize("NFD", texto)
    texto = "".join(c for c in texto if unicodedata.category(c) != "Mn")
    texto = texto.replace("\0", "ñ")
    texto = re.sub(r"[^\w\s]", " ", texto)
    return texto.split()


def distancia_edicion(ref: list[str], hip: list[str]) -> int:
    """Distancia de Levenshtein a nivel de palabras (sustituciones+borrados+inserciones)."""
    anterior = list(range(len(hip) + 1))
    for i, palabra_ref in enumerate(ref, start=1):
        actual = [i] + [0] * len(hip)
        for j, palabra_hip in enumerate(hip, start=1):
            costo = 0 if palabra_ref == palabra_hip else 1
            actual[j] = min(anterior[j] + 1, actual[j - 1] + 1, anterior[j - 1] + costo)
        anterior = actual
    return anterior[len(hip)]


def wer(referencia: str, hipotesis: str) -> float:
    ref = normalizar(referencia)
    hip = normalizar(hipotesis)
    if not ref:
        return 0.0 if not hip else 1.0
    return distancia_edicion(ref, hip) / len(ref)


def leer_lineas(ruta: str) -> list[str]:
    with open(ruta, encoding="utf-8") as f:
        return [linea.rstrip("\n") for linea in f]


def evaluar(referencias: list[str], hipotesis: list[str], umbral: float, meta: int) -> int:
    referencias = [r for r in referencias if r.strip()]
    if len(hipotesis) < len(referencias):
        hipotesis = hipotesis + [""] * (len(referencias) - len(hipotesis))

    total_errores = 0
    total_palabras = 0
    utilizables = 0
    print(f"{'#':>2}  {'WER':>6}  {'OK':^3}  Referencia | Hipótesis")
    for i, (ref, hip) in enumerate(zip(referencias, hipotesis), start=1):
        valor = wer(ref, hip)
        palabras = len(normalizar(ref))
        total_errores += round(valor * palabras)
        total_palabras += palabras
        es_util = valor <= umbral
        utilizables += es_util
        marca = "sí" if es_util else "NO"
        print(f"{i:>2}  {valor:6.2%}  {marca:^3}  {ref} | {hip or '<vacía>'}")

    global_wer = total_errores / total_palabras if total_palabras else 0.0
    n = len(referencias)
    print()
    print(f"WER global: {global_wer:.2%}")
    print(f"Transcripciones utilizables (WER <= {umbral:.0%}): {utilizables}/{n}")
    cumple = utilizables >= meta
    print(f"Meta >= {meta}/{n}: {'CUMPLE' if cumple else 'NO CUMPLE'}")
    return 0 if cumple else 1


def autoprueba() -> int:
    assert normalizar("¿Qué HORA es?") == ["que", "hora", "es"]
    assert normalizar("Año, niño") == ["año", "niño"]
    assert wer("hola mundo", "hola mundo") == 0.0
    assert wer("hola mundo", "hola") == 0.5
    assert wer("hola mundo", "hola mundo cruel") == 0.5
    assert wer("a b c d", "a x c d") == 0.25
    assert wer("", "") == 0.0
    refs = ["uno dos tres", "cuatro cinco"]
    assert evaluar(refs, ["uno dos tres", "cuatro cinco"], 0.3, 2) == 0
    assert evaluar(refs, ["uno dos tres"], 0.3, 2) == 1
    print("Autoprueba de calcular_wer.py: OK")
    return 0


def main() -> int:
    analizador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    analizador.add_argument("referencias", nargs="?")
    analizador.add_argument("hipotesis", nargs="?")
    analizador.add_argument("--umbral", type=float, default=0.30,
                            help="WER máximo para considerar utilizable (0.30 por defecto)")
    analizador.add_argument("--meta", type=int, default=18,
                            help="mínimo de transcripciones utilizables (18 por defecto)")
    analizador.add_argument("--autoprueba", action="store_true")
    args = analizador.parse_args()
    if args.autoprueba:
        return autoprueba()
    if not args.referencias or not args.hipotesis:
        analizador.error("indique REFERENCIAS e HIPOTESIS")
    return evaluar(leer_lineas(args.referencias), leer_lineas(args.hipotesis),
                   args.umbral, args.meta)


if __name__ == "__main__":
    sys.exit(main())
