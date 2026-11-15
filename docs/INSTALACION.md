# Instalación de Jarvis-son V1 paso a paso

Guía para alguien que empieza **desde cero**: un teléfono Android y un computador con Windows.
Cada comando va en su propio bloque; debajo aparece la **salida esperada** (aproximada).

> **Leyenda de verificación**
>
> - ✅ **Verificado en CI/Linux**: probado automáticamente en Linux x86_64 (GitHub Actions o un
>   contenedor Ubuntu 24.04), no en un teléfono.
> - 📱 **Validar en el móvil**: el equipo debe comprobarlo en el teléfono real y anotar el
>   resultado en la [plantilla de reporte](#9-plantilla-para-reportar-resultados).

| Paso | Estado |
|---|---|
| Compilación del orquestador, pruebas unitarias e integración (binarios falsos) | ✅ |
| `instalar.sh` completo (whisper.cpp v1.9.5, llama.cpp v1.0.0, sherpa-onnx v1.13.8) | ✅ en Linux x86_64 · 📱 en Termux |
| `descargar_modelos.sh base` (descarga, SHA-256, extracción de la voz, idempotencia) | ✅ en Linux · 📱 en Termux |
| STT, SLM y TTS reales con los modelos del perfil `base`, **sin red** | ✅ en Linux x86_64 · 📱 en Termux |
| Termux:API (micrófono, `termux-battery-status`), `play-audio`, latencias en 3 GB, modo avión | 📱 |
| Perfil `alto` (Qwen3.5-2B, whisper base, voz high) | 📱 (solo se verificó la configuración en CI) |

---

## Requisitos

- Teléfono Android **13 o superior**, procesador **ARM64**, **3 GB de RAM** o más.
- Unos **3 GB libres** de almacenamiento (perfil `base`: ~700 MB de modelos + ~1,5 GB de
  compilación; perfil `alto`: ~1,6 GB de modelos adicionales).
- Wi-Fi para la instalación (después funciona en modo avión).
- Un computador con Windows en la **misma red Wi-Fi** (para SSH, opcional pero muy recomendado).

---

## 1. Instalar Termux y Termux:API (desde F-Droid)

> ⚠️ **Termux y Termux:API deben venir de la MISMA fuente.** Si una es de F-Droid y la otra de
> Google Play o de GitHub, están firmadas con llaves distintas y `termux-*` se queda colgado. Esta
> guía usa F-Droid para las dos. Si ya tenía Termux de Google Play, desinstálelo primero.

1. En el teléfono, abra el navegador y descargue F-Droid desde <https://f-droid.org>. Instale el
   APK (Android pedirá permitir "instalar apps desconocidas" para el navegador).
2. Abra F-Droid, espere a que actualice los repositorios y busque e instale:
   - **Termux**
   - **Termux:API**
3. **Abra Termux:API una vez** (muestra una pantalla en blanco o un mensaje y se cierra; es normal).
   Esto registra la app en Android.
4. Permisos de Termux:API: *Ajustes → Aplicaciones → Termux:API → Permisos → Micrófono →
   Permitir*.
5. Batería sin restricciones (para que Android no cierre Termux durante compilaciones largas):
   *Ajustes → Aplicaciones → Termux → Batería → **Sin restricciones*** y repita para
   **Termux:API**.
6. Inicio automático, según la marca:
   - **Xiaomi / Redmi / POCO (MIUI/HyperOS):** *Ajustes → Aplicaciones → Administrar aplicaciones
     → Termux → **Inicio automático** (activar)* y *Ahorro de batería → **Sin restricciones***.
     Repita para Termux:API. En "Recientes", mantenga presionada la tarjeta de Termux y toque el
     **candado**.
   - **Samsung (One UI):** *Ajustes → Mantenimiento del dispositivo / Batería → Límites de uso en
     segundo plano → **Aplicaciones que nunca se suspenden** → agregar Termux y Termux:API*.
     Desactive "Poner en suspensión aplicaciones no usadas".
7. Abra **Termux**. La primera vez instala su sistema base (tarda un minuto).

Actualice los paquetes y conceda acceso al almacenamiento:

```bash
pkg update -y && pkg upgrade -y
```

Salida esperada (final):

```
Reading package lists... Done
...
0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.
```

> Si pregunta por archivos de configuración (`*** ... (Y/I/N/O/D/Z) [default=N] ?`), presione
> Enter para conservar la versión actual. Si `pkg update` falla, ejecute `termux-change-repo` y
> elija otro espejo.

```bash
termux-setup-storage
```

Salida esperada: Android muestra un diálogo de permiso de archivos; toque **Permitir**.

---

## 2. Configurar SSH en Termux

Así podrá escribir los comandos desde el computador en lugar del teclado del teléfono.

```bash
pkg install -y openssh termux-api
```

Salida esperada (final):

```
Setting up openssh (...)
Setting up termux-api (...)
```

Cree una contraseña para entrar por SSH (no se ve mientras escribe; escríbala dos veces):

```bash
passwd
```

Salida esperada:

```
New password:
Retype new password:
New password was successfully set.
```

Consulte su nombre de usuario (lo necesitará en PuTTY):

```bash
whoami
```

Salida esperada (el número varía):

```
u0_a245
```

Consulte la IP del teléfono en la red Wi-Fi:

```bash
ifconfig 2>/dev/null | grep -A1 wlan0 | grep 'inet '
```

Salida esperada:

```
        inet 192.168.1.37  netmask 255.255.255.0  broadcast 192.168.1.255
```

> Si no aparece nada, mire la IP en *Ajustes → Wi-Fi → (su red) → Dirección IP*.

Inicie el servidor SSH (Termux usa el puerto **8022**, no el 22):

```bash
sshd
```

Salida esperada: ninguna (si no hay error, ya está corriendo). Para comprobarlo:

```bash
pgrep -a sshd
```

```
12345 sshd
```

Para que `sshd` y el bloqueo de suspensión se activen solos al abrir Termux:

```bash
echo 'pgrep -x sshd >/dev/null || sshd' >> ~/.bashrc
```

```bash
echo 'termux-wake-lock' >> ~/.bashrc
```

Salida esperada: ninguna. Active el bloqueo de suspensión ya mismo (evita que Android congele Termux
con la pantalla apagada; aparece una notificación "wake lock held"):

```bash
termux-wake-lock
```

---

## 3. Conectarse desde Windows con PuTTY y pasar archivos con WinSCP

### PuTTY (terminal)

1. Descargue PuTTY desde <https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html> e
   instálelo.
2. Abra PuTTY y en **Session** escriba:
   - **Host Name (or IP address):** la IP del teléfono (p. ej. `192.168.1.37`)
   - **Port:** `8022`
   - **Connection type:** `SSH`
3. En **Saved Sessions** escriba `jarvis-telefono` y presione **Save** (así no lo vuelve a
   escribir).
4. Presione **Open**. La primera vez aparece una alerta de seguridad de la llave del servidor:
   presione **Accept**.
5. En `login as:` escriba el usuario de `whoami` (p. ej. `u0_a245`) y luego la contraseña de
   `passwd`.

Salida esperada:

```
Welcome to Termux!
...
~ $
```

> **Pegar en PuTTY:** copie el comando en Windows (Ctrl+C) y haga **clic derecho** dentro de
> PuTTY. Seleccionar texto en PuTTY lo copia automáticamente.

### WinSCP (archivos)

1. Descargue WinSCP desde <https://winscp.net> e instálelo.
2. **Nuevo sitio**: Protocolo **SFTP**, Servidor = IP del teléfono, Puerto **8022**, Usuario = el de
   `whoami`, Contraseña = la de `passwd`. Presione **Guardar** y luego **Conectar**.
3. A la derecha verá `/data/data/com.termux/files/home` (su carpeta `~`). Arrastre archivos entre
   ambos paneles.

### Opcional: entrar sin contraseña con una llave (PuTTYgen)

1. Abra **PuTTYgen** (viene con PuTTY), tipo **EdDSA (Ed25519)**, presione **Generate** y mueva el
   mouse.
2. **Save private key** → `jarvis.ppk` (guárdela en un lugar seguro).
3. Copie el texto de "Public key for pasting into OpenSSH authorized_keys file".
4. En PuTTY (ya conectado) ejecute, pegando la llave entre las comillas:

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh
```

```bash
echo 'ssh-ed25519 AAAA...pegue-aqui-su-llave... jarvis' >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
```

5. En PuTTY: *Connection → SSH → Auth → Credentials → Private key file* = `jarvis.ppk`;
   *Connection → Data → Auto-login username* = su usuario. Vuelva a **Session** y presione
   **Save**.

---

## 4. Clonar el repositorio

Primera vez:

```bash
pkg install -y git
```

```bash
git clone https://github.com/JaiderL/jarvis-son.git ~/jarvis-son
```

Salida esperada:

```
Cloning into '/data/data/com.termux/files/home/jarvis-son'...
remote: Enumerating objects: ...
Receiving objects: 100% ...
Resolving deltas: 100% ...
```

```bash
cd ~/jarvis-son
```

Para actualizar después (cuando el equipo publique cambios):

```bash
cd ~/jarvis-son && git pull
```

Salida esperada:

```
Already up to date.
```

(o la lista de archivos actualizados). Si se actualizó código C++, vuelva a ejecutar
`bash scripts/instalar.sh`: solo recompila lo necesario.

---

## 5. Instalar, descargar modelos y probar en orden

### 5.1 Instalar dependencias y compilar

```bash
bash scripts/instalar.sh
```

Tarda **30–90 minutos** la primera vez (compila whisper.cpp, llama.cpp y sherpa-onnx con 2 hilos
para no agotar la RAM). Mantenga el teléfono conectado al cargador y con `termux-wake-lock`.

Salida esperada (final):

```
[ OK ] whisper.cpp compilado: .../third_party/whisper.cpp/build/bin/whisper-cli
[ OK ] llama.cpp compilado: .../third_party/llama.cpp/build/bin/llama-completion
[ OK ] sherpa-onnx compilado: .../third_party/sherpa-onnx/build/bin/sherpa-onnx-offline-tts
100% tests passed, 0 tests failed out of 1
[ OK ] Orquestador listo: .../build/jarvis
[INFO] 7/7 Resumen
[ OK ] whisper-cli responde
version: ... (llama.cpp)
[ OK ] sherpa-onnx-offline-tts responde
jarvis-son 1.0.0
[ OK ] Instalación completa.
Siguiente paso: bash scripts/descargar_modelos.sh base
```

> El script es **idempotente**: si se interrumpe, vuelva a ejecutarlo y continuará donde quedó.
> Si Android cierra la compilación (OOM), use `JOBS=1 bash scripts/instalar.sh`.

📱 **Sobre sherpa-onnx en Termux:** no hay paquete `pkg` de sherpa-onnx ni un binario precompilado
oficial para Termux (los binarios `linux-aarch64` usan glibc y Termux usa Bionic). Por eso
`instalar.sh` lo **compila desde el código fuente** (v1.13.8). Para onnxruntime usa el paquete
`onnxruntime` de Termux si existe; si no, CMake detecta Android y descarga el onnxruntime
precompilado oficial para Android arm64. Ver [DECISIONES.md](DECISIONES.md).

### 5.2 Descargar los modelos del perfil base

```bash
bash scripts/descargar_modelos.sh base
```

Salida esperada (final):

```
[ OK ] SHA-256 correcto: whisper-tiny
[ OK ] SHA-256 correcto: qwen3.5-0.8b-q4_0
[AVISO] voz-es_MX-ald-medium no tiene SHA-256 registrado en el manifiesto.
[AVISO] SHA-256 calculado: c60cf7bc853eb01a875fba0756523fecf9d9f46c763b19785d223fc981fb4fb6
[ OK ] Voz instalada en models/vits-piper-es_MX-ald-medium
[ OK ] Modelos del perfil 'base' listos. Ya puede activar el modo avión.
```

> El aviso de la voz es esperado: k2-fsa no publica el SHA-256 de sus paquetes de voces. Compare el
> valor calculado con el de esta guía; si coincide, regístrelo en `models/manifest.json` con un PR.

### 5.3 Probar Termux:API (micrófono y altavoz)

```bash
bash scripts/probar_api.sh
```

Salida esperada:

```
[INFO] 1/4 Probando comunicación con la app Termux:API (termux-battery-status, máx. 10 s)
[ OK ] Termux:API responde
{"porcentaje":87,"estado":"CHARGING"}
[INFO] 2/4 Grabando 4 s: hable ahora...
[ OK ] Grabación creada (35210 bytes)
[INFO] 3/4 Convirtiendo a WAV PCM s16le, mono, 16 kHz
codec_name=pcm_s16le
sample_rate=16000
channels=1
duration=4.0...
[INFO] Nivel de audio: mean_volume: -32.5 dB (si es menor a -60 dB, la grabación está en silencio)
[INFO] 4/4 Reproduciendo la grabación (suba el volumen multimedia)
[ OK ] Termux:API funciona. Los archivos de prueba se borran al salir.
```

Debe escuchar su propia voz.

### 5.4 Probar STT (whisper)

```bash
bash scripts/probar_stt.sh
```

Diga una frase en español cuando aparezca "Hable ahora". Salida esperada:

```
  Hable ahora (6 s)... listo.
CAPTURA_OK duracion_s=6.00 rms=812.4 ms_grabacion=6900 ms_conversion=180
STT_OK ms=2400 rtf=0.400 rss_mb=140
TRANSCRIPCION: Hola Jarvis, ¿cómo estás hoy?
```

Medición de calidad con las 20 frases de referencia (meta: **≥ 18/20 utilizables**):

```bash
bash scripts/probar_stt.sh --wer
```

Salida esperada (final):

```
WER global: 9.50%
Transcripciones utilizables (WER <= 30%): 19/20
Meta >= 18/20: CUMPLE
```

### 5.5 Probar SLM (Qwen3.5 con llama.cpp)

```bash
bash scripts/probar_slm.sh
```

Salida esperada:

```
[INFO] Perfil base — pregunta: ¿Cuál es la capital de Colombia?
SLM_OK ms=4200 rss_mb=940
RESPUESTA: La capital de Colombia es Bogotá.
[ OK ] SLM funciona. Siguiente paso: bash scripts/probar_tts.sh
```

Con otra pregunta (va entre comillas como **un solo argumento**):

```bash
bash scripts/probar_slm.sh "Dame un consejo para estudiar mejor"
```

### 5.6 Probar TTS (voz Piper es_MX con sherpa-onnx)

```bash
bash scripts/probar_tts.sh
```

Salida esperada (y debe escucharse la frase):

```
TTS_OK motor=sherpa ms_sintesis=1500 ms_reproduccion=4100 rss_mb=165
[ OK ] TTS funciona. Siguiente paso: ./build/jarvis (orquestador completo)
```

Respaldo **solo de demostración** (voz del sistema Android, no Piper):

```bash
bash scripts/probar_tts.sh --respaldo
```

### 5.7 Orquestador completo

```bash
./build/jarvis
```

Salida esperada en cada turno:

```
Jarvis-son 1.0.0 | perfil: base | hilos: 4 | contexto: 1024

[LISTO] Presione Enter para hablar (q + Enter para salir):
[GRABANDO]
  Hable ahora (6 s)... listo.
[STT]
  Usted: ¿Cuál es el río más largo de Colombia?
[SLM]
  Jarvis: El río más largo de Colombia es el Magdalena.
[TTS]
  Tiempos (ms): grabación 6900 | conversión 180 | STT 2400 (RTF 0.40) | SLM 4200 | TTS 1500 | reproducción 4100 | total 19300
  Memoria pico (MB): ffmpeg 45 | STT 140 | SLM 940 | TTS 165
[LISTO]
```

Si algo falla verá, por ejemplo:

```
[ERROR] Etapa: STT | Código de salida: 127
  Detalle: .../whisper-cli: no se pudo lanzar el proceso (No such file or directory)
  Siguiente paso: No se encontró el binario. Ejecute: bash scripts/instalar.sh
```

Salga con `q` + Enter (o Ctrl+C).

### 5.8 Prueba de aceptación (10 turnos)

```bash
bash scripts/prueba_aceptacion.sh
```

Hable en cada turno cuando aparezca "Hable ahora". Salida esperada (final):

```
Turnos analizados: 10 | exitosos: 10 | con error: 0

Etapa                  p50 (s)   p95 (s)   máx (s)
Grabación                 6.90      7.00      7.00
Conversión FFmpeg         0.18      0.25      0.26
STT (whisper)             2.40      3.10      3.20
SLM (llama)               4.20      6.80      7.00
TTS síntesis              1.50      2.10      2.20
Reproducción              4.10      5.00      5.10
Total del turno          19.30     23.50     24.00
RTF STT                   0.40      0.52      0.53

Proceso               memoria pico máx (MB)
ffmpeg                                   45
whisper-cli                             140
llama-completion                        940
sherpa-onnx                             165
[ OK ] Prueba de aceptación completa sin errores.
```

> Los números son ilustrativos: anote los reales en la plantilla de la sección 9.

---

## 6. Cambiar al perfil "alto" (teléfono con mejores características)

Descargue los modelos del perfil alto (~1,5 GB):

```bash
bash scripts/descargar_modelos.sh alto
```

Pruebe cada etapa con el perfil alto:

```bash
bash scripts/probar_slm.sh alto
```

Para usarlo por defecto, cambie `"perfil_activo": "base"` por `"perfil_activo": "alto"` en
`config/config.json`:

```bash
sed -i 's/"perfil_activo": "base"/"perfil_activo": "alto"/' config/config.json
```

o úselo solo una vez:

```bash
./build/jarvis --perfil alto
```

Salida esperada (primera línea):

```
Jarvis-son 1.0.0 | perfil: alto | hilos: 6 | contexto: 2048
```

📱 El perfil alto usa Qwen3.5-2B Q4_K_M (~1,3 GB en disco, ~1,8 GB de RAM estimados). No lo use
en el teléfono de 3 GB.

---

## 7. Prueba en modo avión

1. Con los modelos descargados, active el **modo avión** (Wi-Fi y datos apagados).
2. Ejecute:

```bash
bash scripts/prueba_aceptacion.sh base 3
```

Salida esperada: los 3 turnos terminan en `[LISTO]` sin errores, igual que con red.

✅ En Linux se verificó STT + SLM + TTS reales dentro de un contenedor **sin red**
(`docker run --network none`). 📱 Falta confirmarlo en el teléfono en modo avión.

---

## 8. Problemas comunes

| Síntoma | Causa probable | Solución |
|---|---|---|
| `termux-battery-status` o `probar_api.sh` se queda colgado / "sin respuesta en 10 s" | Falta la **app** Termux:API, no se ha abierto nunca, o Termux y Termux:API vienen de fuentes distintas | Instale ambas desde F-Droid, abra Termux:API una vez, batería "Sin restricciones". Ejecute `pkg install termux-api` |
| La compilación se corta, Termux se cierra o aparece `Killed` / código 137 | Android mató el proceso por falta de RAM (OOM) | Cierre otras apps, `termux-wake-lock`, cargador conectado y `JOBS=1 bash scripts/instalar.sh` (retoma donde quedó) |
| Whisper transcribe en inglés o traduce | Modelo `.en` o falta `-l es` | Use `ggml-tiny.bin` (multilingüe, **no** `ggml-tiny.en.bin`); el orquestador siempre pasa `-l es` y rechaza modelos `.en` |
| `llama-completion`: `unknown model architecture`, `invalid argument` o el modelo no carga | Versión de llama.cpp distinta a la fijada (v1.0.0) o GGUF dañado | `git pull` y `bash scripts/instalar.sh --forzar`; vuelva a ejecutar `descargar_modelos.sh` (verifica SHA-256) |
| La respuesta trae razonamiento en inglés o `<think>` | Se cambió el formato del prompt | El orquestador usa `-no-cnv` con el bloque `<think></think>` vacío; no agregue `--jinja` ni `-sys` (ver DECISIONES.md) |
| No se escucha la voz | Volumen multimedia en 0, falta `play-audio` o la voz no está descargada | Suba el volumen multimedia, `pkg install play-audio`, `bash scripts/probar_tts.sh`. Para descartar el altavoz: `bash scripts/probar_tts.sh --respaldo` |
| `sherpa-onnx`: `phoneme_codepoints is empty ... no frontend available` | Se compiló sherpa-onnx v2.x (quitó espeak-ng) | Use la versión fijada v1.13.8: `bash scripts/instalar.sh --forzar` |
| "grabación vacía" o RMS ~0 | Sin permiso de micrófono, otra app usa el micrófono o se habló muy lejos | *Ajustes → Apps → Termux:API → Permisos → Micrófono*. Cierre apps de llamadas/grabación. `bash scripts/probar_api.sh` |
| "duración ... fuera del rango [5.0, 8.0]" | La grabación se cortó antes | Revise que Termux:API no esté restringida por batería; reintente |
| `pkg update` falla o es muy lento | Espejo caído | `termux-change-repo` y elija otro espejo |
| PuTTY: "Connection refused" | `sshd` no está corriendo o IP cambió | En Termux: `sshd`; verifique la IP con `ifconfig` |
| `Illegal instruction` al ejecutar whisper/llama | Optimización nativa (`-mcpu=native`) no soportada por el SoC | `GGML_NATIVE=OFF bash scripts/instalar.sh --forzar` (y repórtelo en un issue) |

---

## 9. Plantilla para reportar resultados

Copie esta plantilla en un issue (tipo "Reporte de problema" o un comentario del PR) y complétela.
**No adjunte audio ni el texto de las conversaciones.**

```markdown
### Resultados Jarvis-son V1

| Campo | Valor |
|---|---|
| Teléfono (marca y modelo) | |
| SoC (procesador) | |
| RAM | |
| Versión de Android | |
| Origen de Termux / Termux:API | F-Droid / F-Droid |
| Commit probado (`git rev-parse --short HEAD`) | |
| Perfil | base / alto |
| Modelo STT | ggml-tiny.bin (F16) |
| Modelo SLM y cuantización | Qwen3.5-0.8B Q4_0 |
| Voz TTS | es_MX-ald-medium |
| Modo avión probado | sí / no |

#### Latencias (de `bash scripts/prueba_aceptacion.sh`)

| Etapa | p50 (s) | p95 (s) |
|---|---|---|
| Grabación | | |
| Conversión FFmpeg | | |
| STT (whisper) | | |
| SLM (llama) | | |
| TTS síntesis | | |
| Reproducción | | |
| **Total** | | |
| RTF STT | | |

#### Memoria pico máxima

| Proceso | MB |
|---|---|
| whisper-cli | |
| llama-completion | |
| sherpa-onnx | |

#### Calidad STT (`bash scripts/probar_stt.sh --wer`)

- Transcripciones utilizables: __/20 (meta ≥ 18)
- WER global: __ %

#### Problemas observados

- 
```
