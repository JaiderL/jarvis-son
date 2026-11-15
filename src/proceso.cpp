// Implementación del lanzador de procesos con fork/execvp y wait4.
#include "jarvis/proceso.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>

namespace jarvis {

volatile std::sig_atomic_t g_interrumpido = 0;

namespace {

void manejar_senal(int) {
    g_interrumpido = 1;
}

using Reloj = std::chrono::steady_clock;

long ms_desde(Reloj::time_point inicio) {
    return static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Reloj::now() - inicio).count());
}

bool marcar_cloexec(int fd) {
    int banderas = fcntl(fd, F_GETFD);
    return banderas >= 0 && fcntl(fd, F_SETFD, banderas | FD_CLOEXEC) == 0;
}

void cerrar(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

// Lee lo disponible de fd; devuelve false si llegó EOF o error definitivo.
bool leer_disponible(int fd, std::string& destino, std::size_t maximo) {
    char bufer[4096];
    ssize_t n = read(fd, bufer, sizeof bufer);
    if (n > 0) {
        std::size_t libres = destino.size() < maximo ? maximo - destino.size() : 0;
        destino.append(bufer,
                       static_cast<std::size_t>(n) < libres ? static_cast<std::size_t>(n) : libres);
        return true;
    }
    if (n < 0 && (errno == EINTR || errno == EAGAIN)) return true;
    return false;
}

void registrar_estado(ResultadoProceso& r, int estado, const struct rusage& uso) {
    if (WIFEXITED(estado)) r.codigo_salida = WEXITSTATUS(estado);
    if (WIFSIGNALED(estado)) r.senal = WTERMSIG(estado);
    r.memoria_pico_kb = uso.ru_maxrss;
}

}  // namespace

void instalar_manejador_senales() {
    struct sigaction accion;
    std::memset(&accion, 0, sizeof accion);
    accion.sa_handler = manejar_senal;
    sigemptyset(&accion.sa_mask);
    sigaction(SIGINT, &accion, nullptr);
    sigaction(SIGTERM, &accion, nullptr);
}

int ResultadoProceso::codigo_reporte() const {
    if (!lanzado) return 127;
    if (timeout) return 124;
    if (interrumpido) return 130;
    if (senal != 0) return 128 + senal;
    return codigo_salida;
}

std::string ResultadoProceso::describir() const {
    std::ostringstream os;
    if (!lanzado) {
        os << "no se pudo lanzar el proceso (" << std::strerror(errno_lanzamiento) << ")";
    } else if (timeout) {
        os << "superó el tiempo límite y fue terminado";
    } else if (interrumpido) {
        os << "interrumpido por el usuario";
    } else if (senal != 0) {
        os << "terminado por la señal " << senal;
    } else {
        os << "código de salida " << codigo_salida;
    }
    return os.str();
}

ResultadoProceso ejecutar(const std::vector<std::string>& argv, const OpcionesProceso& op) {
    ResultadoProceso r;
    if (argv.empty() || argv[0].empty()) {
        r.errno_lanzamiento = EINVAL;
        return r;
    }

    // Se prepara todo lo que reserva memoria ANTES de fork().
    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    const std::string ruta_stdin = op.archivo_stdin.empty() ? "/dev/null" : op.archivo_stdin;

    int tubo_out[2] = {-1, -1};
    int tubo_err[2] = {-1, -1};
    int tubo_exec[2] = {-1, -1};  // informa errno si execvp falla
    if (pipe(tubo_out) != 0 || pipe(tubo_err) != 0 || pipe(tubo_exec) != 0) {
        r.errno_lanzamiento = errno;
        for (int* t : {tubo_out, tubo_err, tubo_exec}) {
            cerrar(t[0]);
            cerrar(t[1]);
        }
        return r;
    }
    for (int fd : {tubo_out[0], tubo_err[0], tubo_exec[0], tubo_exec[1]}) marcar_cloexec(fd);

    const auto inicio = Reloj::now();
    pid_t pid = fork();
    if (pid < 0) {
        r.errno_lanzamiento = errno;
        for (int* t : {tubo_out, tubo_err, tubo_exec}) {
            cerrar(t[0]);
            cerrar(t[1]);
        }
        return r;
    }

    if (pid == 0) {
        // Proceso hijo: grupo propio para poder terminar también a sus nietos.
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        int fd_in = open(ruta_stdin.c_str(), O_RDONLY);
        if (fd_in < 0 || dup2(fd_in, STDIN_FILENO) < 0 || dup2(tubo_out[1], STDOUT_FILENO) < 0 ||
            dup2(tubo_err[1], STDERR_FILENO) < 0) {
            int e = errno;
            ssize_t ignorado = write(tubo_exec[1], &e, sizeof e);
            (void)ignorado;
            _exit(127);
        }
        close(fd_in);
        close(tubo_out[1]);
        close(tubo_err[1]);
        execvp(args[0], args.data());
        int e = errno;
        ssize_t ignorado = write(tubo_exec[1], &e, sizeof e);
        (void)ignorado;
        _exit(127);
    }

    // Proceso padre.
    setpgid(pid, pid);  // evita la carrera con el setpgid del hijo
    cerrar(tubo_out[1]);
    cerrar(tubo_err[1]);
    cerrar(tubo_exec[1]);

    int errno_hijo = 0;
    ssize_t leidos;
    do {
        leidos = read(tubo_exec[0], &errno_hijo, sizeof errno_hijo);
    } while (leidos < 0 && errno == EINTR);
    cerrar(tubo_exec[0]);
    r.lanzado = (leidos == 0);
    if (!r.lanzado) r.errno_lanzamiento = errno_hijo != 0 ? errno_hijo : ENOEXEC;

    int fd_out = tubo_out[0];
    int fd_err = tubo_err[0];
    int estado = 0;
    struct rusage uso;
    std::memset(&uso, 0, sizeof uso);
    bool terminado = false;
    auto fin_hijo = Reloj::now();

    for (;;) {
        struct pollfd fds[2];
        nfds_t n = 0;
        if (fd_out >= 0) fds[n++] = {fd_out, POLLIN, 0};
        if (fd_err >= 0) fds[n++] = {fd_err, POLLIN, 0};
        if (n > 0) {
            int listo = poll(fds, n, 100);
            if (listo > 0) {
                for (nfds_t i = 0; i < n; ++i) {
                    if (fds[i].revents == 0) continue;
                    bool es_out = (fds[i].fd == fd_out);
                    bool sigue =
                        leer_disponible(fds[i].fd, es_out ? r.salida : r.error, op.max_captura);
                    if (!sigue) cerrar(es_out ? fd_out : fd_err);
                }
            }
        } else {
            usleep(20000);
        }

        if (!terminado) {
            pid_t w = wait4(pid, &estado, WNOHANG, &uso);
            if (w == pid) {
                terminado = true;
                fin_hijo = Reloj::now();
                registrar_estado(r, estado, uso);
            }
        }
        // Tras terminar el hijo se dan 300 ms para vaciar los tubos.
        if (terminado && ((fd_out < 0 && fd_err < 0) || ms_desde(fin_hijo) > 300)) break;

        const bool vencido = ms_desde(inicio) >= op.timeout_ms;
        if (!terminado && (vencido || g_interrumpido)) {
            r.timeout = vencido;
            r.interrumpido = !vencido;
            kill(-pid, SIGTERM);
            kill(pid, SIGTERM);
            const auto inicio_gracia = Reloj::now();
            while (ms_desde(inicio_gracia) < 1500) {
                if (wait4(pid, &estado, WNOHANG, &uso) == pid) {
                    terminado = true;
                    break;
                }
                usleep(50000);
            }
            if (!terminado) {
                kill(-pid, SIGKILL);
                kill(pid, SIGKILL);
                while (wait4(pid, &estado, 0, &uso) < 0 && errno == EINTR) {
                }
            }
            registrar_estado(r, estado, uso);
            break;
        }
    }
    cerrar(fd_out);
    cerrar(fd_err);
    r.duracion_ms = ms_desde(inicio);
    return r;
}

}  // namespace jarvis
