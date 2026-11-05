#include "audio/dsp/jsfx/NovaJsfxYsfxEngine.h"

#include <ysfx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

struct NovaJsfxYsfxEngine::Impl {
    struct SliderHandle { int sliderNum = 0; };

    ysfx_config_t *config = nullptr;
    ysfx_t *fx = nullptr;
    std::string ultimoError;

    std::vector<std::unique_ptr<SliderHandle>> sliders;
    std::map<int, int> sliderNumToIndex;
    std::vector<int> sliderNumsOrden;

    double tempo = 120.0;
    double playPos = 0.0;
    int playState = 0;
    int numCh = 2;

    Impl()
    {
        config = ysfx_config_new();
        ysfx_set_import_root(config, ".");
        ysfx_set_data_root(config, ".");
        ysfx_register_builtin_audio_formats(config);
        ysfx_set_log_reporter(config, &Impl::logReporter);
        ysfx_set_user_data(config, reinterpret_cast<intptr_t>(this));
    }

    ~Impl()
    {
        if (fx) ysfx_free(fx);
        if (config) ysfx_config_free(config);
    }

    static void logReporter(intptr_t ud, ysfx_log_level level, const char *message)
    {
        if (level == ysfx_log_error && ud != 0) {
            auto *self = reinterpret_cast<Impl *>(ud);
            self->ultimoError = (message != nullptr) ? message : "";
        }
    }

    void liberarFx()
    {
        if (fx) {
            ysfx_free(fx);
            fx = nullptr;
        }
        sliderNumToIndex.clear();
        sliderNumsOrden.clear();
    }

    // Extrae los numeros de slider ("sliderN:") en orden de aparicion.
    static std::vector<int> extraerSliders(const std::string &fuente)
    {
        std::vector<int> out;
        std::regex re(R"(slider(\d+)\s*:)");
        std::smatch m;
        std::string::const_iterator it = fuente.begin();
        while (std::regex_search(it, fuente.end(), m, re)) {
            out.push_back(std::stoi(m[1].str()));
            it = m.suffix().first;
        }
        return out;
    }

    bool finalizarCarga(const std::string &fuente, std::string *outError)
    {
        ultimoError.clear();
        if (!ysfx_compile(fx, ysfx_compile_no_gfx)) {
            if (outError) {
                *outError = ultimoError.empty() ? "ysfx: error de compilacion" : ultimoError;
            }
            liberarFx();
            return false;
        }

            sliderNumsOrden = extraerSliders(fuente);
        sliderNumToIndex.clear();
        for (size_t i = 0; i < sliderNumsOrden.size(); ++i) {
            sliderNumToIndex[sliderNumsOrden[i]] = static_cast<int>(i);
        }
        return true;
    }
};

NovaJsfxYsfxEngine::NovaJsfxYsfxEngine()
    : m_impl(std::make_unique<Impl>())
{
}

NovaJsfxYsfxEngine::~NovaJsfxYsfxEngine() = default;

bool NovaJsfxYsfxEngine::cargar(const std::string &fuente, std::string *outError)
{
    m_impl->liberarFx();
    m_impl->ultimoError.clear();

    // YSFX solo carga desde fichero. Se vuelca la fuente a un temporal.
    char plantilla[] = "/tmp/nova_jsfx_XXXXXX";
    int fd = mkstemp(plantilla);
    if (fd < 0) {
        if (outError) *outError = "ysfx: no se pudo crear el fichero temporal";
        return false;
    }
    {
        std::ofstream out(plantilla, std::ios::binary | std::ios::trunc);
        out.write(fuente.data(), static_cast<std::streamsize>(fuente.size()));
    }
    close(fd);

    std::string dirTemporal = plantilla;
    const size_t barra = dirTemporal.find_last_of("/\\");
    if (barra != std::string::npos) dirTemporal = dirTemporal.substr(0, barra);
    ysfx_set_import_root(m_impl->config, dirTemporal.c_str());
    ysfx_set_data_root(m_impl->config, dirTemporal.c_str());

    m_impl->fx = ysfx_new(m_impl->config);
    if (!m_impl->fx) {
        std::remove(plantilla);
        if (outError) *outError = "ysfx: no se pudo crear el efecto";
        return false;
    }

    const bool ok = ysfx_load_file(m_impl->fx, plantilla, 0);
    // std::remove(plantilla);
    if (!ok) {
        if (outError) {
            *outError = m_impl->ultimoError.empty()
                            ? "ysfx: no se pudo cargar el script"
                            : m_impl->ultimoError;
        }
        m_impl->liberarFx();
        return false;
    }

    if (!m_impl->finalizarCarga(fuente, outError)) {
        return false;
    }
    return true;
}

void *NovaJsfxYsfxEngine::registrarVariable(const std::string &nombre)
{
    // Solo los sliders se registran desde la fachada. Se guarda el numero
    // para resolver el indice de YSFX tras la carga.
    if (nombre.rfind("slider", 0) != 0) return nullptr;
    const std::string num = nombre.substr(6);
    int n = 0;
    try { n = std::stoi(num); } catch (...) { n = 0; }
    if (n <= 0) return nullptr;

auto handle = std::make_unique<Impl::SliderHandle>();
handle->sliderNum = n;
    void *ptr = handle.get();
    m_impl->sliders.push_back(std::move(handle));
    return ptr;
}

void NovaJsfxYsfxEngine::asignarVariable(void *variable, double valor)
{
    if (!variable || !m_impl->fx) return;
    auto *h = static_cast<Impl::SliderHandle *>(variable);
    auto it = m_impl->sliderNumToIndex.find(h->sliderNum);
    if (it == m_impl->sliderNumToIndex.end()) return;
    ysfx_slider_set_value(m_impl->fx, static_cast<uint32_t>(it->second),
                         static_cast<ysfx_real>(valor), false);
}

double NovaJsfxYsfxEngine::leerVariable(const void *variable) const
{
    if (!variable || !m_impl->fx) return 0.0;
    auto *h = static_cast<const Impl::SliderHandle *>(variable);
    auto it = m_impl->sliderNumToIndex.find(h->sliderNum);
    if (it == m_impl->sliderNumToIndex.end()) return 0.0;
    return static_cast<double>(ysfx_slider_get_value(
        m_impl->fx, static_cast<uint32_t>(it->second)));
}

void NovaJsfxYsfxEngine::ejecutarInit()
{
    if (!m_impl->fx) return;
    // aplicar transporte antes de @init
    ysfx_time_info_t ti{};
    ti.tempo = static_cast<ysfx_real>(m_impl->tempo);
    ti.playback_state = (m_impl->playState == 1) ? ysfx_playback_playing : ysfx_playback_stopped;
    ti.time_position = static_cast<ysfx_real>(m_impl->playPos);
    ti.beat_position = static_cast<ysfx_real>(m_impl->playPos * m_impl->tempo / 60.0);
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_init(m_impl->fx);
    ysfx_set_time_info(m_impl->fx, &ti);
}

void NovaJsfxYsfxEngine::ejecutarSlider()
{
    if (!m_impl->fx) return;
    // asegurar que el transporte esta aplicado antes de @slider
    ysfx_time_info_t ti{};
    ti.tempo = static_cast<ysfx_real>(m_impl->tempo);
    ti.playback_state = (m_impl->playState == 1) ? ysfx_playback_playing : ysfx_playback_stopped;
    ti.time_position = static_cast<ysfx_real>(m_impl->playPos);
    ti.beat_position = static_cast<ysfx_real>(m_impl->playPos * m_impl->tempo / 60.0);
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_set_time_info(m_impl->fx, &ti);
    // ysfx_init deja must_compute_slider a true; si no hubo init reciente,
    // forzar la notificacion de al menos un slider para ejecutar @slider.
    if (!m_impl->sliderNumsOrden.empty()) {
        int idx = m_impl->sliderNumToIndex.count(m_impl->sliderNumsOrden.front())
                      ? m_impl->sliderNumToIndex[m_impl->sliderNumsOrden.front()]
                      : 0;
        ysfx_real v = ysfx_slider_get_value(m_impl->fx, static_cast<uint32_t>(idx));
        ysfx_slider_set_value(m_impl->fx, static_cast<uint32_t>(idx), v + 1e-12, true);
        ysfx_slider_set_value(m_impl->fx, static_cast<uint32_t>(idx), v, true);
    }
    // Ejecutar @slider y @block sin producir muestras.
    const uint32_t nins = std::max(1u, ysfx_get_num_inputs(m_impl->fx));
    const uint32_t nouts = std::max(1u, ysfx_get_num_outputs(m_impl->fx));
    std::vector<float> dummyIn(nins, 0.0f), dummyOut(nouts, 0.0f);
    std::vector<const float *> ins(nins);
    std::vector<float *> outs(nouts);
    for (uint32_t i = 0; i < nins; ++i) ins[i] = dummyIn.data() + i; // no, planar needs separate; keep simple
    // planar simple: un canal
    const float *insArr[1] = { dummyIn.data() };
    float *outsArr[1] = { dummyOut.data() };
    uint32_t ni = 1, no = 1;
    if (nins > 1) ni = nins;
    if (nouts > 1) no = nouts;
    std::vector<std::vector<float>> inBufs, outBufs;
    if (ni > 1 || no > 1) {
        inBufs.assign(ni, std::vector<float>(1, 0.0f));
        outBufs.assign(no, std::vector<float>(1, 0.0f));
        std::vector<const float *> inPtrs(ni);
        std::vector<float *> outPtrs(no);
        for (uint32_t i = 0; i < ni; ++i) inPtrs[i] = inBufs[i].data();
        for (uint32_t i = 0; i < no; ++i) outPtrs[i] = outBufs[i].data();
        try {
            ysfx_process_float(m_impl->fx, inPtrs.data(), outPtrs.data(), ni, no, 0);
            fflush(stderr);
        } catch (...) {
            fflush(stderr);
        }
    } else {
        ysfx_process_float(m_impl->fx, insArr, outsArr, 1, 1, 0);
    }
}

void NovaJsfxYsfxEngine::ejecutarBlock() {}
void NovaJsfxYsfxEngine::ejecutarSample() {}

std::string NovaJsfxYsfxEngine::guardarEstado()
{
    if (!m_impl->fx) return {};
    ysfx_state_t *st = ysfx_save_state(m_impl->fx);
    if (!st) return {};
    std::string out;
    if (st->data && st->data_size > 0) {
        out.assign(reinterpret_cast<const char *>(st->data), st->data_size);
    }
    ysfx_state_free(st);
    return out;
}

bool NovaJsfxYsfxEngine::cargarEstado(const std::string &estado)
{
    if (!m_impl->fx || estado.empty()) return false;
    ysfx_state_t st{};
    st.data = reinterpret_cast<uint8_t *>(const_cast<char *>(estado.data()));
    st.data_size = estado.size();
    st.sliders = nullptr;
    st.slider_count = 0;
    // Guardar sliders actuales (restaurados por el gestor de parches) para
    // no perderlos: ysfx_load_state los reinicia a los valores por defecto.
    std::vector<ysfx_real> slidersGuardados;
    uint32_t nsl = 0;
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
        if (ysfx_slider_exists(m_impl->fx, i)) ++nsl;
    slidersGuardados.resize(nsl);
    for (uint32_t i = 0; i < nsl; ++i)
        slidersGuardados[i] = ysfx_slider_get_value(m_impl->fx, i);
    bool r = ysfx_load_state(m_impl->fx, &st);
    for (uint32_t i = 0; i < nsl; ++i)
        ysfx_slider_set_value(m_impl->fx, i, slidersGuardados[i], false);
    return r;
}

bool NovaJsfxYsfxEngine::tieneSerialize() const
{
    return m_impl->fx && ysfx_has_section(m_impl->fx, ysfx_section_serialize);
}

bool NovaJsfxYsfxEngine::tieneBlock() const
{
    return m_impl->fx && ysfx_has_section(m_impl->fx, ysfx_section_block);
}

bool NovaJsfxYsfxEngine::tieneSample() const
{
    return m_impl->fx && ysfx_has_section(m_impl->fx, ysfx_section_sample);
}

void NovaJsfxYsfxEngine::limpiarMemoria() {}
void NovaJsfxYsfxEngine::liberarMemoriaSolicitada() {}

void NovaJsfxYsfxEngine::setSampleRate(double srate)
{
    if (m_impl->fx) ysfx_set_sample_rate(m_impl->fx, static_cast<ysfx_real>(srate));
}

void NovaJsfxYsfxEngine::setSamplesBlock(int muestras)
{
    if (m_impl->fx) ysfx_set_block_size(m_impl->fx, static_cast<uint32_t>(std::max(1, muestras)));
}

void NovaJsfxYsfxEngine::setTempo(double bpm)
{
    m_impl->tempo = bpm > 0 ? bpm : 120.0;
}

void NovaJsfxYsfxEngine::setPlayPosition(double segundos)
{
    m_impl->playPos = segundos;
}

void NovaJsfxYsfxEngine::setNumChannels(int canales)
{
    m_impl->numCh = std::max(1, std::min(canales, 2));
}

void NovaJsfxYsfxEngine::setPlayState(int estado)
{
    m_impl->playState = estado;
    if (m_impl->fx) {
        ysfx_time_info_t ti{};
        ti.tempo = static_cast<ysfx_real>(m_impl->tempo);
        ti.playback_state = (m_impl->playState == 1) ? ysfx_playback_playing : ysfx_playback_stopped;
        ti.time_position = static_cast<ysfx_real>(m_impl->playPos);
        ti.beat_position = 0.0;
        ti.time_signature[0] = 4;
        ti.time_signature[1] = 4;
        ysfx_set_time_info(m_impl->fx, &ti);
    }
}

void NovaJsfxYsfxEngine::procesarBloque(float *interleavedBuffer, int numFrames, int channels)
{
    fflush(stderr);
    if (!m_impl->fx || !interleavedBuffer || numFrames <= 0) return;

    const int effCh = std::min(channels, m_impl->numCh);
    const uint32_t nch = static_cast<uint32_t>(std::max(1, effCh));

    // Aplicar transporte antes del proceso.
    ysfx_time_info_t ti{};
    ti.tempo = static_cast<ysfx_real>(m_impl->tempo);
    ti.playback_state = (m_impl->playState == 1) ? ysfx_playback_playing : ysfx_playback_stopped;
    ti.time_position = static_cast<ysfx_real>(m_impl->playPos);
    ti.beat_position = static_cast<ysfx_real>(m_impl->playPos * m_impl->tempo / 60.0);
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_set_time_info(m_impl->fx, &ti);

    // Deintercalar a planar.
    std::vector<std::vector<float>> inBufs(nch, std::vector<float>(numFrames, 0.0f));
    std::vector<std::vector<float>> outBufs(nch, std::vector<float>(numFrames, 0.0f));
    for (int f = 0; f < numFrames; ++f) {
        for (uint32_t c = 0; c < nch; ++c) {
            inBufs[c][f] = interleavedBuffer[f * channels + c];
        }
    }
    std::vector<const float *> ins(nch);
    std::vector<float *> outs(nch);
    for (uint32_t c = 0; c < nch; ++c) {
        ins[c] = inBufs[c].data();
        outs[c] = outBufs[c].data();
    }

    ysfx_process_float(m_impl->fx, ins.data(), outs.data(), nch, nch,
                       static_cast<uint32_t>(numFrames));
    // Reintercalar.
    for (int f = 0; f < numFrames; ++f) {
        for (uint32_t c = 0; c < nch; ++c) {
            interleavedBuffer[f * channels + c] = outBufs[c][f];
        }
    }
}

void NovaJsfxYsfxEngine::procesarMuestra(float *izquierda, float *derecha)
{
    // Compatibilidad: procesa un solo fotograma.
    if (!izquierda) return;
    float buf[2] = { *izquierda, derecha ? *derecha : *izquierda };
    procesarBloque(buf, 1, 2);
    *izquierda = buf[0];
    if (derecha) *derecha = buf[1];
}
