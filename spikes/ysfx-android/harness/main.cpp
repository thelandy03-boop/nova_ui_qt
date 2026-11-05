// Banco de pruebas del spike YSFX para Android.
//
// No usa Qt: es un ejecutable autonómo que se compila igual en Linux y para
// Android con el NDK. Mide los cinco criterios de salida del spike:
//
//   1. Carga y compilación de un script con sliders y @sample.
//   2. Procesado de audio estéreo a 48 kHz con resultado correcto.
//   3. Persistencia del estado interno de @serialize.
//   4. @gfx pintando sobre un framebuffer BGRA.
//   5. @import, strings y MIDI (superficie que el motor propio no cubre).
//
// Todos los textos van en español, como el resto del repo.

#include "ysfx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_comprobaciones = 0;
int g_fallos = 0;
// Fotogramas que procesa el modo --uno antes de ejecutar @gfx. Permite
// comprobar si un fallo depende del número de muestras procesadas.
int g_frames_uno = 256;

void comprobar(bool condicion, const std::string &nombre,
               const std::string &detalle = std::string())
{
    ++g_comprobaciones;
    if (condicion) {
        std::printf("[ OK ] %s\n", nombre.c_str());
        return;
    }
    ++g_fallos;
    if (detalle.empty())
        std::printf("[FALLO] %s\n", nombre.c_str());
    else
        std::printf("[FALLO] %s — %s\n", nombre.c_str(), detalle.c_str());
}

void aviso(const std::string &texto)
{
    std::printf("[AVISO] %s\n", texto.c_str());
}

// ---------------------------------------------------------------------------
// Utilidades de ficheros y rutas (sin std::filesystem, para no depender de
// detalles del NDK en API 31).
// ---------------------------------------------------------------------------

std::string unirRuta(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

bool existeFichero(const std::string &ruta)
{
    FILE *f = std::fopen(ruta.c_str(), "rb");
    if (!f)
        return false;
    std::fclose(f);
    return true;
}

std::vector<uint8_t> leerFichero(const std::string &ruta, bool *ok)
{
    std::vector<uint8_t> datos;
    FILE *f = std::fopen(ruta.c_str(), "rb");
    if (!f) {
        if (ok)
            *ok = false;
        return datos;
    }
    std::fseek(f, 0, SEEK_END);
    const long tam = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (tam > 0) {
        datos.resize(static_cast<size_t>(tam));
        const size_t leidos = std::fread(datos.data(), 1, datos.size(), f);
        datos.resize(leidos);
    }
    std::fclose(f);
    if (ok)
        *ok = true;
    return datos;
}

bool escribirFichero(const std::string &ruta, const void *datos, size_t tam)
{
    FILE *f = std::fopen(ruta.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = tam == 0 || std::fwrite(datos, 1, tam, f) == tam;
    std::fclose(f);
    return ok;
}

// ---------------------------------------------------------------------------
// Registro de YSFX
// ---------------------------------------------------------------------------

void registrarLog(intptr_t, ysfx_log_level nivel, const char *mensaje)
{
    std::printf("[ysfx:%s] %s\n", ysfx_log_level_string(nivel), mensaje);
}

struct LiberadorConfig {
    void operator()(ysfx_config_t *p) const { ysfx_config_free(p); }
};

struct LiberadorEfecto {
    void operator()(ysfx_t *p) const { ysfx_free(p); }
};

struct LiberadorEstado {
    void operator()(ysfx_state_t *p) const { ysfx_state_free(p); }
};

using ConfigPtr = std::unique_ptr<ysfx_config_t, LiberadorConfig>;
using EfectoPtr = std::unique_ptr<ysfx_t, LiberadorEfecto>;
using EstadoPtr = std::unique_ptr<ysfx_state_t, LiberadorEstado>;

ConfigPtr crearConfig(const std::string &raiz)
{
    ConfigPtr config{ysfx_config_new()};
    if (!config)
        return config;
    ysfx_set_log_reporter(config.get(), &registrarLog);
    ysfx_set_import_root(config.get(), raiz.c_str());
    ysfx_set_data_root(config.get(), raiz.c_str());
    return config;
}

bool cargarYCompilar(ysfx_t *fx, const std::string &ruta)
{
    if (!ysfx_load_file(fx, ruta.c_str(), 0)) {
        std::printf("[ysfx] no se pudo cargar: %s\n", ruta.c_str());
        return false;
    }
    if (!ysfx_compile(fx, 0)) {
        std::printf("[ysfx] no se pudo compilar: %s\n", ruta.c_str());
        return false;
    }
    return true;
}

void inicializar(ysfx_t *fx, double frecuencia, uint32_t bloque)
{
    ysfx_set_sample_rate(fx, frecuencia);
    ysfx_set_block_size(fx, bloque);
    ysfx_init(fx);
}

double rms(const std::vector<float> &datos)
{
    if (datos.empty())
        return 0.0;
    double suma = 0.0;
    for (float v : datos)
        suma += static_cast<double>(v) * v;
    return std::sqrt(suma / static_cast<double>(datos.size()));
}

// Genera una senoide de amplitud 0.5 en ambos canales.
void llenarSenoide(std::vector<float> &izq, std::vector<float> &der,
                   double frecuencia, double tasa)
{
    for (size_t i = 0; i < izq.size(); ++i) {
        const double fase = 2.0 * M_PI * frecuencia * static_cast<double>(i) / tasa;
        izq[i] = static_cast<float>(0.5 * std::sin(fase));
        der[i] = izq[i];
    }
}

int contarSliders(ysfx_t *fx)
{
    int total = 0;
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        if (ysfx_slider_exists(fx, i))
            ++total;
    }
    return total;
}

// ---------------------------------------------------------------------------
// 1 y 2. Audio estéreo a 48 kHz
// ---------------------------------------------------------------------------

void probarAudio(const std::string &raiz, const std::string &salida)
{
    std::printf("\n== Audio estéreo (%s) ==\n", "48 kHz");
    const double tasa = 48000.0;
    const uint32_t bloque = 256;

    ConfigPtr config = crearConfig(raiz);
    comprobar(static_cast<bool>(config), "config de YSFX creada");
    if (!config)
        return;

    EfectoPtr fx{ysfx_new(config.get())};
    comprobar(static_cast<bool>(fx), "efecto creado");
    if (!fx)
        return;

    const std::string script = unirRuta(raiz, "audio_ganancia.jsfx");
    comprobar(cargarYCompilar(fx.get(), script), "audio_ganancia.jsfx carga y compila");
    comprobar(ysfx_is_loaded(fx.get()), "el efecto queda cargado");
    comprobar(ysfx_is_compiled(fx.get()), "el efecto queda compilado");
    comprobar(ysfx_has_section(fx.get(), ysfx_section_sample), "tiene sección @sample");

    comprobar(contarSliders(fx.get()) == 2, "se enumeran los 2 sliders del script");
    const char *nombre0 = ysfx_slider_get_name(fx.get(), 0);
    comprobar(nombre0 && std::strcmp(nombre0, "Gain") == 0,
              "el slider 1 se llama Gain", nombre0 ? nombre0 : "(null)");

    // Curva del slider: default 1.0, rango 0..1.
    ysfx_slider_curve_t curva{};
    const bool tieneCurva = ysfx_slider_get_curve(fx.get(), 0, &curva);
    comprobar(tieneCurva && std::fabs(curva.def - 1.0) < 1e-9
                  && std::fabs(curva.min - 0.0) < 1e-9
                  && std::fabs(curva.max - 1.0) < 1e-9,
              "la curva del slider 1 es 0..1 con default 1");

    inicializar(fx.get(), tasa, bloque);
    comprobar(std::fabs(ysfx_get_sample_rate(fx.get()) - tasa) < 1e-9,
              "la tasa de muestreo es 48000");
    comprobar(ysfx_get_block_size(fx.get()) == bloque, "el bloque es 256");

    // Procesa unos bloques para que cualquier estado interno se asiente.
    std::vector<float> entradaIzq(bloque), entradaDer(bloque);
    std::vector<float> salidaIzq(bloque), salidaDer(bloque);
    llenarSenoide(entradaIzq, entradaDer, 440.0, tasa);

    const float *entradas[2] = {entradaIzq.data(), entradaDer.data()};
    float *salidas[2] = {salidaIzq.data(), salidaDer.data()};

    ysfx_process_float(fx.get(), entradas, salidas, 2, 2, bloque);

    const double rmsEntrada = rms(entradaIzq);
    const double rmsSalida = rms(salidaIzq);
    comprobar(rmsEntrada > 0.3, "la señal de entrada tiene nivel",
              std::to_string(rmsEntrada));
    comprobar(std::fabs(rmsSalida - rmsEntrada) < 0.01,
              "con ganancia 1.0 la salida conserva el nivel",
              "entrada=" + std::to_string(rmsEntrada) + " salida=" + std::to_string(rmsSalida));

    // Cambia el slider a 0.5 y vuelve a procesar: debe pasar por @slider.
    ysfx_slider_set_value(fx.get(), 0, 0.5, true);
    ysfx_process_float(fx.get(), entradas, salidas, 2, 2, bloque);
    const double rmsMitad = rms(salidaIzq);
    comprobar(std::fabs(rmsMitad - rmsEntrada * 0.5) < 0.01,
              "con ganancia 0.5 la salida baja a la mitad",
              "esperado=" + std::to_string(rmsEntrada * 0.5) + " salida=" + std::to_string(rmsMitad));

    // Un efecto que no toca el audio debe devolver la señal intacta.
    EfectoPtr fx2{ysfx_new(config.get())};
    if (fx2 && cargarYCompilar(fx2.get(), unirRuta(raiz, "audio_paso.jsfx"))) {
        inicializar(fx2.get(), tasa, bloque);
        std::vector<float> outIzq(bloque), outDer(bloque);
        float *salidas2[2] = {outIzq.data(), outDer.data()};
        ysfx_process_float(fx2.get(), entradas, salidas2, 2, 2, bloque);
        double error = 0.0;
        for (uint32_t i = 0; i < bloque; ++i) {
            error += std::fabs(outIzq[i] - entradaIzq[i]);
            error += std::fabs(outDer[i] - entradaDer[i]);
        }
        error /= 2.0 * bloque;
        comprobar(error < 1e-7, "el efecto de paso no altera la señal",
                  "error medio=" + std::to_string(error));
    } else {
        comprobar(false, "audio_paso.jsfx carga y compila");
    }

    // Medida de rendimiento orientativa (no es un criterio eliminatorio).
    const int bloques = 200;
    const auto inicio = std::chrono::steady_clock::now();
    for (int i = 0; i < bloques; ++i)
        ysfx_process_float(fx.get(), entradas, salidas, 2, 2, bloque);
    const auto fin = std::chrono::steady_clock::now();
    const double segundos = std::chrono::duration<double>(fin - inicio).count();
    const double muestras = static_cast<double>(bloques) * bloque * 2.0;
    const double nsPorMuestra = segundos * 1e9 / muestras;
    std::printf("[INFO] rendimiento: %.1f ns/muestra (%.1f Msps estéreo)\n",
                nsPorMuestra, 1000.0 / nsPorMuestra);

    (void)salida;
}

// ---------------------------------------------------------------------------
// 3. @serialize
// ---------------------------------------------------------------------------

void probarSerialize(const std::string &raiz, const std::string &salida)
{
    std::printf("\n== Estado interno con @serialize ==\n");
    const double tasa = 48000.0;
    const uint32_t bloque = 64;

    ConfigPtr config = crearConfig(raiz);
    if (!config) {
        comprobar(false, "config de YSFX creada");
        return;
    }

    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx) {
        comprobar(false, "efecto creado");
        return;
    }

    const std::string script = unirRuta(raiz, "serialize.jsfx");
    comprobar(cargarYCompilar(fx.get(), script), "serialize.jsfx carga y compila");
    comprobar(ysfx_has_section(fx.get(), ysfx_section_serialize),
              "el script declara @serialize");

    inicializar(fx.get(), tasa, bloque);

    // El script suma 1 a `contador` por muestra procesada.
    std::vector<float> in(bloque, 0.0f);
    std::vector<float> outIzq(bloque, 0.0f), outDer(bloque, 0.0f);
    const float *entradas[2] = {in.data(), in.data()};
    float *salidas[2] = {outIzq.data(), outDer.data()};
    ysfx_process_float(fx.get(), entradas, salidas, 2, 2, bloque);
    comprobar(std::fabs(ysfx_read_var(fx.get(), "contador") - bloque) < 1e-9,
              "tras un bloque, el contador interno vale 64",
              std::to_string(ysfx_read_var(fx.get(), "contador")));

    EstadoPtr estado{ysfx_save_state(fx.get())};
    comprobar(static_cast<bool>(estado), "ysfx_save_state devuelve estado");
    if (!estado)
        return;

    std::printf("[INFO] estado: %u sliders, %zu bytes serializados\n",
                estado->slider_count, estado->data_size);
    comprobar(estado->slider_count == 1, "el estado incluye el slider");
    comprobar(estado->data_size > 0, "el estado incluye el blob de @serialize");

    const std::string ficheroEstado = unirRuta(salida, "serialize_estado.bin");
    comprobar(escribirFichero(ficheroEstado, estado->data, estado->data_size),
              "el blob se escribe en disco", ficheroEstado);
    comprobar(existeFichero(ficheroEstado), "el fichero de estado existe");

    // Instancia nueva: @init vuelve a poner el contador a 0; después se carga
    // el estado como haría Nova al abrir un .novapatch.
    EfectoPtr fx2{ysfx_new(config.get())};
    if (!fx2 || !cargarYCompilar(fx2.get(), script)) {
        comprobar(false, "la segunda instancia carga el script");
        return;
    }
    inicializar(fx2.get(), tasa, bloque);
    comprobar(std::fabs(ysfx_read_var(fx2.get(), "contador")) < 1e-9,
              "la instancia nueva empieza en 0");

    bool okLectura = false;
    std::vector<uint8_t> blob = leerFichero(ficheroEstado, &okLectura);
    comprobar(okLectura && !blob.empty(), "el blob se lee de disco");
    if (!blob.empty()) {
        ysfx_state_t cargado{};
        cargado.data = blob.data();
        cargado.data_size = blob.size();
        const bool okCarga = ysfx_load_state(fx2.get(), &cargado);
        comprobar(okCarga, "ysfx_load_state acepta el blob");
        comprobar(std::fabs(ysfx_read_var(fx2.get(), "contador") - bloque) < 1e-9,
                  "el contador se restaura a 64",
                  std::to_string(ysfx_read_var(fx2.get(), "contador")));
        EstadoPtr reguardado{ysfx_save_state(fx2.get())};
        comprobar(reguardado && reguardado->data_size == estado->data_size
                      && std::memcmp(reguardado->data, estado->data, estado->data_size) == 0,
                  "guardar-cargar-guardar produce el mismo blob");
    }
}

// ---------------------------------------------------------------------------
// 4. @gfx sobre framebuffer BGRA
// ---------------------------------------------------------------------------

void escribirPPM(const std::string &ruta, const std::vector<uint8_t> &bgra,
                 int ancho, int alto, int stride)
{
    FILE *f = std::fopen(ruta.c_str(), "wb");
    if (!f)
        return;
    std::fprintf(f, "P6\n%d %d\n255\n", ancho, alto);
    for (int y = 0; y < alto; ++y) {
        const uint8_t *fila = bgra.data() + static_cast<size_t>(y) * stride;
        for (int x = 0; x < ancho; ++x) {
            const uint8_t *p = fila + static_cast<size_t>(x) * 4;
            const uint8_t rgb[3] = {p[2], p[1], p[0]};
            std::fwrite(rgb, 1, 3, f);
        }
    }
    std::fclose(f);
}

void probarGfx(const std::string &raiz, const std::string &salida)
{
    std::printf("\n== @gfx sobre framebuffer BGRA ==\n");
    const int ancho = 320;
    const int alto = 200;
    const int stride = ancho * 4;

    ConfigPtr config = crearConfig(raiz);
    if (!config) {
        comprobar(false, "config de YSFX creada");
        return;
    }

    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx) {
        comprobar(false, "efecto creado");
        return;
    }

    const std::string script = unirRuta(raiz, "gfx.jsfx");
    const bool cargado = cargarYCompilar(fx.get(), script);
    comprobar(cargado, "gfx.jsfx carga y compila");
    comprobar(ysfx_has_section(fx.get(), ysfx_section_gfx),
              "el script declara @gfx");
    if (!cargado)
        return;

    inicializar(fx.get(), 48000.0, 256);

    std::vector<uint8_t> framebuffer(static_cast<size_t>(stride) * alto, 0);
    ysfx_gfx_config_t gc{};
    gc.user_data = nullptr;
    gc.pixel_width = ancho;
    gc.pixel_height = alto;
    gc.pixel_stride = stride;
    gc.pixels = framebuffer.data();
    gc.scale_factor = 1.0;

    ysfx_gfx_setup(fx.get(), &gc);
    ysfx_gfx_set_window_state(fx.get(), true, true, true);
    // Coloca el ratón lejos de la esquina para no tapar el fondo.
    ysfx_gfx_update_mouse(fx.get(), 0, 10, 10, 0, 0.0, 0.0);

    bool pintado = false;
    for (int i = 0; i < 3; ++i)
        pintado = ysfx_gfx_run(fx.get()) || pintado;

    comprobar(pintado, "ysfx_gfx_run devuelve que el framebuffer cambió");

    // gfx_w y gfx_h deben reflejar el tamaño del canvas (bug abierto #165 en
    // YSFX: gfx_h no siempre se actualiza; el test lo deja en evidencia).
    comprobar(std::fabs(ysfx_read_var(fx.get(), "ancho_visto") - ancho) < 1e-9,
              "gfx_w vale 320", std::to_string(ysfx_read_var(fx.get(), "ancho_visto")));
    comprobar(std::fabs(ysfx_read_var(fx.get(), "alto_visto") - alto) < 1e-9,
              "gfx_h vale 200", std::to_string(ysfx_read_var(fx.get(), "alto_visto")));

    // Fondo azul oscuro: (0.1, 0.2, 0.3) -> B=76, G=51, R=25.
    const uint8_t *fondo = framebuffer.data();
    comprobar(std::abs(fondo[0] - 76) <= 4 && std::abs(fondo[1] - 51) <= 4
                  && std::abs(fondo[2] - 25) <= 4,
              "el fondo se pinta con el color esperado");

    // Rectángulo naranja en (20,20)-(140,100).
    const uint8_t *naranja = framebuffer.data() + 40 * stride + 40 * 4;
    comprobar(naranja[2] > 200 && naranja[1] > 120 && naranja[0] < 80,
              "el rectángulo naranja está pintado");

    // Círculo verde en (220,100).
    const uint8_t *verde = framebuffer.data() + 100 * stride + 220 * 4;
    comprobar(verde[1] > 180 && verde[2] < 100 && verde[0] < 100,
              "el círculo verde está pintado");

    // Texto blanco: cuenta píxeles casi blancos. Sin backend de fuentes en
    // Android el texto no se dibuja; el aviso es intencionado.
    int blancos = 0;
    for (int y = 140; y < 190; ++y) {
        const uint8_t *fila = framebuffer.data() + static_cast<size_t>(y) * stride;
        for (int x = 0; x < 300; ++x) {
            const uint8_t *p = fila + static_cast<size_t>(x) * 4;
            if (p[0] > 230 && p[1] > 230 && p[2] > 230)
                ++blancos;
        }
    }
    if (blancos < 20)
        aviso("el texto no se ha renderizado (posible falta de backend de fuentes)");
    else
        comprobar(true, "el texto se renderiza en el framebuffer");

    const std::string ppm = unirRuta(salida, "gfx.ppm");
    escribirPPM(ppm, framebuffer, ancho, alto, stride);
    comprobar(existeFichero(ppm), "el framebuffer se vuelca a PPM", ppm);

    // Entrada de ratón: el script pinta una marca donde está el cursor.
    ysfx_gfx_update_mouse(fx.get(), 0, 160, 60, 0, 0.0, 0.0);
    ysfx_gfx_run(fx.get());
    const uint8_t *cursor = framebuffer.data() + 60 * stride + 160 * 4;
    comprobar(cursor[0] > 200 && cursor[1] > 200 && cursor[2] < 80,
              "gfx_x/gfx_y siguen al ratón");
}

// ---------------------------------------------------------------------------
// 5. @import, strings y MIDI
// ---------------------------------------------------------------------------

void probarImportStrings(const std::string &raiz, const std::string &salida)
{
    std::printf("\n== @import y funciones de cadena ==\n");
    (void)salida;
    ConfigPtr config = crearConfig(raiz);
    if (!config) {
        comprobar(false, "config de YSFX creada");
        return;
    }

    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx) {
        comprobar(false, "efecto creado");
        return;
    }

    const std::string script = unirRuta(raiz, "imports.jsfx");
    const bool cargado = cargarYCompilar(fx.get(), script);
    comprobar(cargado, "imports.jsfx carga y compila (@import)");
    if (!cargado)
        return;
    inicializar(fx.get(), 48000.0, 64);

    std::vector<float> in(64, 0.25f);
    std::vector<float> outIzq(64, 0.0f), outDer(64, 0.0f);
    const float *entradas[2] = {in.data(), in.data()};
    float *salidas[2] = {outIzq.data(), outDer.data()};
    ysfx_process_float(fx.get(), entradas, salidas, 2, 2, 64);

    comprobar(std::fabs(ysfx_read_var(fx.get(), "largo") - 5) < 1e-9,
              "strlen(\"NOVA!\") vale 5",
              std::to_string(ysfx_read_var(fx.get(), "largo")));
    comprobar(std::fabs(ysfx_read_var(fx.get(), "suma") - 7) < 1e-9,
              "la función del fichero importado devuelve 7",
              std::to_string(ysfx_read_var(fx.get(), "suma")));
    comprobar(std::fabs(outIzq[0] - 0.5f) < 1e-6,
              "el audio pasa por la función importada");
}

void probarMidi(const std::string &raiz, const std::string &salida)
{
    std::printf("\n== MIDI ==\n");
    (void)salida;
    ConfigPtr config = crearConfig(raiz);
    if (!config) {
        comprobar(false, "config de YSFX creada");
        return;
    }

    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx) {
        comprobar(false, "efecto creado");
        return;
    }

    const std::string script = unirRuta(raiz, "midi.jsfx");
    const bool cargado = cargarYCompilar(fx.get(), script);
    comprobar(cargado, "midi.jsfx carga y compila");
    if (!cargado)
        return;
    inicializar(fx.get(), 48000.0, 64);

    const uint8_t nota[3] = {0x90, 60, 100};
    ysfx_midi_event_t evento{};
    evento.bus = 0;
    evento.offset = 10;
    evento.size = 3;
    evento.data = nota;
    comprobar(ysfx_send_midi(fx.get(), &evento), "ysfx_send_midi acepta la nota");

    std::vector<float> in(64, 0.0f);
    std::vector<float> outIzq(64, 0.0f), outDer(64, 0.0f);
    const float *entradas[2] = {in.data(), in.data()};
    float *salidas[2] = {outIzq.data(), outDer.data()};
    ysfx_process_float(fx.get(), entradas, salidas, 2, 2, 64);

    ysfx_midi_event_t recibido{};
    const bool hay = ysfx_receive_midi(fx.get(), &recibido);
    comprobar(hay, "ysfx_receive_midi devuelve la nota reenviada");
    if (hay) {
        comprobar(recibido.offset == 10 && recibido.size == 3,
                  "el evento conserva offset y tamaño");
        comprobar(recibido.data && recibido.data[0] == 0x90
                      && recibido.data[1] == 60 && recibido.data[2] == 100,
                  "el mensaje MIDI llega intacto");
    }
    comprobar(std::fabs(ysfx_read_var(fx.get(), "notas") - 1) < 1e-9,
              "el script contó 1 nota MIDI",
              std::to_string(ysfx_read_var(fx.get(), "notas")));
}

// ---------------------------------------------------------------------------
// Informe de secciones
// ---------------------------------------------------------------------------

void informarSecciones(const std::string &raiz)
{
    std::printf("\n== Secciones detectadas ==\n");
    ConfigPtr config = crearConfig(raiz);
    if (!config)
        return;
    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx)
        return;
    if (!cargarYCompilar(fx.get(), unirRuta(raiz, "audio_ganancia.jsfx")))
        return;

    struct Par {
        uint32_t tipo;
        const char *nombre;
    };
    const Par pares[] = {
        {ysfx_section_init, "@init"},
        {ysfx_section_slider, "@slider"},
        {ysfx_section_block, "@block"},
        {ysfx_section_sample, "@sample"},
        {ysfx_section_gfx, "@gfx"},
        {ysfx_section_serialize, "@serialize"},
    };
    for (const Par &p : pares)
        std::printf("[INFO] %-10s %s\n", p.nombre,
                    ysfx_has_section(fx.get(), p.tipo) ? "sí" : "no");
}

// ---------------------------------------------------------------------------
// Modo corpus: carga un único script y emite una línea parseable.
// ---------------------------------------------------------------------------

std::string seccionesDe(ysfx_t *fx)
{
    struct Par {
        uint32_t tipo;
        const char *nombre;
    };
    const Par pares[] = {
        {ysfx_section_init, "@init"},
        {ysfx_section_slider, "@slider"},
        {ysfx_section_block, "@block"},
        {ysfx_section_sample, "@sample"},
        {ysfx_section_gfx, "@gfx"},
        {ysfx_section_serialize, "@serialize"},
    };
    std::string texto;
    for (const Par &p : pares) {
        if (!ysfx_has_section(fx, p.tipo))
            continue;
        if (!texto.empty())
            texto += ",";
        texto += p.nombre;
    }
    return texto.empty() ? "-" : texto;
}

int probarUno(const std::string &ruta, const std::string &raiz)
{
    ConfigPtr config = crearConfig(raiz);
    if (!config) {
        std::printf("CORPUS|%s|carga=error|compila=no|audio=no|gfx=no|sliders=0|secciones=-\n",
                    ruta.c_str());
        return 3;
    }

    EfectoPtr fx{ysfx_new(config.get())};
    if (!fx) {
        std::printf("CORPUS|%s|carga=error|compila=no|audio=no|gfx=no|sliders=0|secciones=-\n",
                    ruta.c_str());
        return 3;
    }

    const bool cargado = ysfx_load_file(fx.get(), ruta.c_str(), 0);
    if (!cargado) {
        std::printf("CORPUS|%s|carga=error|compila=no|audio=no|gfx=no|sliders=0|secciones=-\n",
                    ruta.c_str());
        return 10;
    }

    const bool compilado = ysfx_compile(fx.get(), 0);
    if (!compilado) {
        std::printf("CORPUS|%s|carga=ok|compila=error|audio=no|gfx=no|sliders=0|secciones=-\n",
                    ruta.c_str());
        return 11;
    }

    inicializar(fx.get(), 48000.0, 256);

    // Audio: una senoide baja; solo se comprueba que la salida sea finita.
    // Muchos scripts son sintetizadores o necesitan MIDI y devuelven silencio,
    // que también es una salida válida.
    const size_t marcos = static_cast<size_t>(std::max(0, g_frames_uno));
    std::vector<float> in(marcos, 0.0f);
    std::vector<float> outIzq(marcos, 0.0f), outDer(marcos, 0.0f);
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = static_cast<float>(0.1 * std::sin(2.0 * M_PI * 440.0 * i / 48000.0));
    const float *entradas[2] = {in.data(), in.data()};
    float *salidas[2] = {outIzq.data(), outDer.data()};
    if (marcos > 0)
        ysfx_process_float(fx.get(), entradas, salidas, 2, 2, static_cast<uint32_t>(marcos));

    bool audioFinito = true;
    for (size_t i = 0; i < outIzq.size() && audioFinito; ++i) {
        if (!std::isfinite(outIzq[i]) || !std::isfinite(outDer[i]))
            audioFinito = false;
    }

    std::string gfx = "no";
    if (ysfx_has_section(fx.get(), ysfx_section_gfx)) {
        // El canvas se dimensiona con lo que declara el script (`@gfx w h`),
        // que es lo que hace REAPER. Si no declara tamaño, 320x200.
        uint32_t dim[2] = {320, 200};
        if (ysfx_get_gfx_dim(fx.get(), dim) && (dim[0] == 0 || dim[1] == 0)) {
            dim[0] = 320;
            dim[1] = 200;
        }
        const int ancho = static_cast<int>(std::min<uint32_t>(dim[0], 2048));
        const int alto = static_cast<int>(std::min<uint32_t>(dim[1], 2048));
        std::vector<uint8_t> framebuffer(static_cast<size_t>(ancho) * alto * 4, 0);
        ysfx_gfx_config_t gc{};
        gc.pixel_width = ancho;
        gc.pixel_height = alto;
        gc.pixel_stride = ancho * 4;
        gc.pixels = framebuffer.data();
        gc.scale_factor = 1.0;
        ysfx_gfx_setup(fx.get(), &gc);
        ysfx_gfx_set_window_state(fx.get(), true, true, true);
        bool pintado = false;
        for (int i = 0; i < 3; ++i)
            pintado = ysfx_gfx_run(fx.get()) || pintado;
        gfx = pintado ? "ok" : "sin_cambio";
    }

    std::printf("CORPUS|%s|carga=ok|compila=ok|audio=%s|gfx=%s|sliders=%d|secciones=%s\n",
                ruta.c_str(), audioFinito ? "ok" : "nan", gfx.c_str(),
                contarSliders(fx.get()), seccionesDe(fx.get()).c_str());
    return audioFinito ? 0 : 12;
}

} // namespace

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    std::string raiz = NOVA_SPIKE_SCRIPTS_DIR;
    std::string salida = ".";
    std::string uno;
    std::string raizUno;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--scripts") == 0 && i + 1 < argc) {
            raiz = argv[++i];
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            salida = argv[++i];
        } else if (std::strcmp(argv[i], "--uno") == 0 && i + 1 < argc) {
            uno = argv[++i];
        } else if (std::strcmp(argv[i], "--raiz-uno") == 0 && i + 1 < argc) {
            raizUno = argv[++i];
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            g_frames_uno = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf("Uso: %s [--scripts <dir>] [--out <dir>]\n", argv[0]);
            std::printf("     %s --uno <script> [--raiz-uno <dir>] [--frames N]\n", argv[0]);
            return 0;
        } else {
            std::printf("Opción desconocida: %s\n", argv[i]);
            return 2;
        }
    }

    if (!uno.empty()) {
        std::string raizEfectiva = raizUno;
        if (raizEfectiva.empty()) {
            const size_t barra = uno.find_last_of('/');
            raizEfectiva = (barra == std::string::npos) ? "." : uno.substr(0, barra);
            if (raizEfectiva.empty())
                raizEfectiva = "/";
        }
        return probarUno(uno, raizEfectiva);
    }

    std::printf("NOVA Studio — spike YSFX\n");
    std::printf("Scripts: %s\n", raiz.c_str());
    std::printf("Salida:  %s\n", salida.c_str());

    if (!existeFichero(unirRuta(raiz, "audio_ganancia.jsfx"))) {
        std::printf("No se encuentra audio_ganancia.jsfx en %s\n", raiz.c_str());
        return 2;
    }

    probarAudio(raiz, salida);
    probarSerialize(raiz, salida);
    probarGfx(raiz, salida);
    probarImportStrings(raiz, salida);
    probarMidi(raiz, salida);
    informarSecciones(raiz);

    std::printf("\n========================================\n");
    std::printf("Comprobaciones: %d, fallos: %d\n", g_comprobaciones, g_fallos);
    std::printf("========================================\n");
    return g_fallos == 0 ? 0 : 1;
}
