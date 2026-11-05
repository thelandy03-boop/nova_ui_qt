#include "audio/dsp/jsfx/NovaJsfxWdlEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// WDL es C. Sus cabeceras ya llevan los guards de extern "C" donde toca.
// eel_fft.h usa los tipos internos de ns-eel-int.h (__NSEEL_RAMAlloc), asi
// que hay que incluirlo antes.
#include "ns-eel.h"
#include "ns-eel-int.h"
#include "ns-eel-addfuncs.h"
#include "eel_fft.h"

namespace {

// WDL usa estos dos stubs para proteger la memoria compartida entre VMs.
// Hay que definirlos (no son opcionales) y deben tener enlace C.
std::mutex g_mutexWdl;

std::once_flag g_funcionesRegistradas;

/**
 * Backend de file_var()/file_mem()/file_avail()/file_rewind() para @serialize.
 *
 * REAPER guarda el estado interno como valores de 32 bits en modo escritura
 * y lo devuelve en modo lectura. El puntero a esta estructura se pasa a las
 * funciones del host mediante NSEEL_VM_SetCustomFuncThis(), de modo que cada
 * VM tiene su propio buffer y no hay estado global compartido.
 */
struct BackendSerializacion {
    std::vector<unsigned char> datos;
    size_t cursor = 0;
    bool escribiendo = false;
    bool activo = false;
    NSEEL_VMCTX vm = nullptr;

    void iniciarEscritura()
    {
        datos.clear();
        cursor = 0;
        escribiendo = true;
        activo = true;
    }

    void iniciarLectura(const std::string &estado)
    {
        datos.assign(estado.begin(), estado.end());
        cursor = 0;
        escribiendo = false;
        activo = true;
    }

    std::string finalizar()
    {
        activo = false;
        if (datos.empty()) return {};
        return std::string(reinterpret_cast<const char *>(datos.data()), datos.size());
    }

    bool escribir(EEL_F valor)
    {
        if (!escribiendo) return false;

        const float compacto = static_cast<float>(valor);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &compacto, sizeof(bits));
        datos.push_back(static_cast<unsigned char>(bits & 0xffu));
        datos.push_back(static_cast<unsigned char>((bits >> 8) & 0xffu));
        datos.push_back(static_cast<unsigned char>((bits >> 16) & 0xffu));
        datos.push_back(static_cast<unsigned char>((bits >> 24) & 0xffu));
        cursor = datos.size();
        return true;
    }

    bool leer(EEL_F *destino)
    {
        if (!destino || escribiendo || cursor + 4 > datos.size()) {
            if (destino) *destino = 0.0;
            if (!escribiendo) cursor = datos.size();
            return false;
        }

        const std::uint32_t bits =
            static_cast<std::uint32_t>(datos[cursor])
            | (static_cast<std::uint32_t>(datos[cursor + 1]) << 8)
            | (static_cast<std::uint32_t>(datos[cursor + 2]) << 16)
            | (static_cast<std::uint32_t>(datos[cursor + 3]) << 24);
        cursor += 4;

        float compacto = 0.0f;
        std::memcpy(&compacto, &bits, sizeof(compacto));
        *destino = static_cast<EEL_F>(compacto);
        return true;
    }

    void rewind() { cursor = 0; }
};

// Funciones que WDL no trae y que los scripts generados por asistentes usan
// con frecuencia. Son funciones normales del host: reciben punteros a los
// argumentos ya evaluados, igual que las funciones de eel_misc.h.
extern "C" {

static EEL_F NSEEL_CGEN_CALL nova_tanh(void *, EEL_F *x)
{
    return std::tanh(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_sinh(void *, EEL_F *x)
{
    return std::sinh(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_cosh(void *, EEL_F *x)
{
    return std::cosh(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_log2(void *, EEL_F *x)
{
    return std::log2(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_exp2(void *, EEL_F *x)
{
    return std::exp2(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_round(void *, EEL_F *x)
{
    return std::round(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_trunc(void *, EEL_F *x)
{
    return std::trunc(*x);
}

static EEL_F NSEEL_CGEN_CALL nova_clamp(void *, EEL_F *x, EEL_F *minimo, EEL_F *maximo)
{
    if (*x < *minimo) return *minimo;
    if (*x > *maximo) return *maximo;
    return *x;
}

static EEL_F NSEEL_CGEN_CALL nova_lerp(void *, EEL_F *a, EEL_F *b, EEL_F *t)
{
    return *a + (*b - *a) * *t;
}

// ── @serialize: handle 0 apunta al buffer interno de la VM ──────────────

static EEL_F NSEEL_CGEN_CALL nova_file_var(void *thisptr, EEL_F *handle, EEL_F *valor)
{
    auto *backend = static_cast<BackendSerializacion *>(thisptr);
    if (!backend || !backend->activo || !valor) return 0.0;
    if (static_cast<int>(std::lround(*handle)) != 0) return 0.0;

    if (backend->escribiendo) {
        return backend->escribir(*valor) ? 1.0 : 0.0;
    }
    return backend->leer(valor) ? 1.0 : 0.0;
}

static EEL_F NSEEL_CGEN_CALL nova_file_mem(void *thisptr, EEL_F *handle, EEL_F *offset,
                                            EEL_F *longitud)
{
    auto *backend = static_cast<BackendSerializacion *>(thisptr);
    if (!backend || !backend->activo || !backend->vm) return 0.0;
    if (static_cast<int>(std::lround(*handle)) != 0) return 0.0;

    const long offsetInicial = std::lround(*offset);
    const long total = std::lround(*longitud);
    if (offsetInicial < 0 || total <= 0) return 0.0;

    long procesados = 0;
    while (procesados < total) {
        int disponibles = 0;
        EEL_F *ram = NSEEL_VM_getramptr(
            backend->vm,
            static_cast<unsigned int>(offsetInicial + procesados),
            &disponibles);
        if (!ram || disponibles <= 0) break;

        const long restantes = total - procesados;
        const int lote = static_cast<int>(std::min<long>(restantes, disponibles));

        if (backend->escribiendo) {
            for (int i = 0; i < lote; ++i) {
                if (!backend->escribir(ram[i])) return procesados;
            }
        } else {
            for (int i = 0; i < lote; ++i) {
                if (!backend->leer(&ram[i])) return procesados;
            }
        }
        procesados += lote;
    }

    return static_cast<EEL_F>(procesados);
}

static EEL_F NSEEL_CGEN_CALL nova_file_avail(void *thisptr, EEL_F *handle)
{
    auto *backend = static_cast<BackendSerializacion *>(thisptr);
    if (!backend || !backend->activo) return 0.0;
    if (static_cast<int>(std::lround(*handle)) != 0) return 0.0;

    // Negativo en escritura; bytes restantes en lectura, como REAPER.
    if (backend->escribiendo) return -1.0;
    return static_cast<EEL_F>(backend->datos.size() - backend->cursor);
}

static EEL_F *NSEEL_CGEN_CALL nova_file_rewind(void *thisptr, EEL_F *handle)
{
    auto *backend = static_cast<BackendSerializacion *>(thisptr);
    if (!backend || !backend->activo) return handle;
    if (static_cast<int>(std::lround(*handle)) != 0) return handle;

    backend->rewind();
    return handle;
}

} // extern "C"

void registrarFuncionesGlobales()
{
    std::call_once(g_funcionesRegistradas, [] {
        // Registra fft/ifft/convolve_c y prepara las tablas de WDL una sola
        // vez por proceso (~395 KB de BSS que solo se tocan al usarse).
        EEL_fft_register();

        NSEEL_addfunc_retval("tanh", 1, NSEEL_PProc_THIS, &nova_tanh);
        NSEEL_addfunc_retval("sinh", 1, NSEEL_PProc_THIS, &nova_sinh);
        NSEEL_addfunc_retval("cosh", 1, NSEEL_PProc_THIS, &nova_cosh);
        NSEEL_addfunc_retval("log2", 1, NSEEL_PProc_THIS, &nova_log2);
        NSEEL_addfunc_retval("exp2", 1, NSEEL_PProc_THIS, &nova_exp2);
        NSEEL_addfunc_retval("round", 1, NSEEL_PProc_THIS, &nova_round);
        NSEEL_addfunc_retval("trunc", 1, NSEEL_PProc_THIS, &nova_trunc);

        // Extras habituales en el codigo que escriben los asistentes. No
        // forman parte del JSFX clasico, pero no rompen ningun script valido.
        NSEEL_addfunc_retval("clamp", 3, NSEEL_PProc_THIS, &nova_clamp);
        NSEEL_addfunc_retval("lerp", 3, NSEEL_PProc_THIS, &nova_lerp);

        // Estado interno de @serialize. El handle 0 apunta al buffer de la VM
        // que ejecuta el codigo (NSEEL_PProc_THIS inyecta su backend).
        NSEEL_addfunc_retval("file_var", 2, NSEEL_PProc_THIS, &nova_file_var);
        NSEEL_addfunc_retval("file_mem", 3, NSEEL_PProc_THIS, &nova_file_mem);
        NSEEL_addfunc_retval("file_avail", 1, NSEEL_PProc_THIS, &nova_file_avail);
        NSEEL_addfunc_retptr("file_rewind", 1, NSEEL_PProc_THIS, &nova_file_rewind);
    });
}

} // namespace

extern "C" void NSEEL_HOSTSTUB_EnterMutex()
{
    g_mutexWdl.lock();
}

extern "C" void NSEEL_HOSTSTUB_LeaveMutex()
{
    g_mutexWdl.unlock();
}

struct NovaJsfxWdlEngine::Impl {
    NSEEL_VMCTX vm = nullptr;

    NSEEL_CODEHANDLE init = nullptr;
    NSEEL_CODEHANDLE slider = nullptr;
    NSEEL_CODEHANDLE block = nullptr;
    NSEEL_CODEHANDLE sample = nullptr;
    NSEEL_CODEHANDLE serialize = nullptr;

    BackendSerializacion backendEstado;

    EEL_F *spl0 = nullptr;
    EEL_F *spl1 = nullptr;
    EEL_F *srate = nullptr;
    EEL_F *samplesblock = nullptr;
    EEL_F *tempo = nullptr;
    EEL_F *playPosition = nullptr;
    EEL_F *beatPosition = nullptr;
    EEL_F *numChannels = nullptr;
    EEL_F *playState = nullptr;
    EEL_F *tsNum = nullptr;
    EEL_F *tsDenom = nullptr;

    void liberarProgramas()
    {
        // Orden inverso al de compilacion: las funciones comunes de @init
        // pueden estar referenciadas por los code handles posteriores.
        if (serialize) {
            NSEEL_code_free(serialize);
            serialize = nullptr;
        }
        if (sample) {
            NSEEL_code_free(sample);
            sample = nullptr;
        }
        if (block) {
            NSEEL_code_free(block);
            block = nullptr;
        }
        if (slider) {
            NSEEL_code_free(slider);
            slider = nullptr;
        }
        if (init) {
            NSEEL_code_free(init);
            init = nullptr;
        }
    }
};

NovaJsfxWdlEngine::NovaJsfxWdlEngine()
    : m_impl(std::make_unique<Impl>())
{
    registrarFuncionesGlobales();

    m_impl->vm = NSEEL_VM_alloc();
    if (!m_impl->vm) return;

    // Las funciones de file_* reciben este puntero para leer/escribir el
    // buffer de @serialize de esta VM concreta.
    m_impl->backendEstado.vm = m_impl->vm;
    NSEEL_VM_SetCustomFuncThis(m_impl->vm, &m_impl->backendEstado);

    m_impl->spl0 = NSEEL_VM_regvar(m_impl->vm, "spl0");
    m_impl->spl1 = NSEEL_VM_regvar(m_impl->vm, "spl1");
    m_impl->srate = NSEEL_VM_regvar(m_impl->vm, "srate");
    m_impl->samplesblock = NSEEL_VM_regvar(m_impl->vm, "samplesblock");
    m_impl->tempo = NSEEL_VM_regvar(m_impl->vm, "tempo");
    m_impl->playPosition = NSEEL_VM_regvar(m_impl->vm, "play_position");
    m_impl->beatPosition = NSEEL_VM_regvar(m_impl->vm, "beat_position");
    m_impl->numChannels = NSEEL_VM_regvar(m_impl->vm, "num_ch");
    m_impl->playState = NSEEL_VM_regvar(m_impl->vm, "play_state");
    m_impl->tsNum = NSEEL_VM_regvar(m_impl->vm, "ts_num");
    m_impl->tsDenom = NSEEL_VM_regvar(m_impl->vm, "ts_denom");

    if (m_impl->srate) *m_impl->srate = 48000.0;
    if (m_impl->samplesblock) *m_impl->samplesblock = 512.0;
    if (m_impl->tempo) *m_impl->tempo = 120.0;
    if (m_impl->playPosition) *m_impl->playPosition = 0.0;
    if (m_impl->beatPosition) *m_impl->beatPosition = 0.0;
    if (m_impl->numChannels) *m_impl->numChannels = 2.0;
    if (m_impl->playState) *m_impl->playState = 0.0;
    if (m_impl->tsNum) *m_impl->tsNum = 4.0;
    if (m_impl->tsDenom) *m_impl->tsDenom = 4.0;
}

NovaJsfxWdlEngine::~NovaJsfxWdlEngine()
{
    if (!m_impl) return;

    m_impl->liberarProgramas();
    if (m_impl->vm) {
        NSEEL_VM_free(m_impl->vm);
        m_impl->vm = nullptr;
    }
}

bool NovaJsfxWdlEngine::compilar(const Programas &programas, std::string *outError)
{
    if (!m_impl || !m_impl->vm) {
        if (outError) *outError = "No se pudo crear la VM EEL2 de WDL";
        return false;
    }

    // Si se reutiliza el motor, se liberan los handles anteriores antes de
    // recompilar. El efecto crea un motor nuevo por script, pero la clase
    // sigue siendo segura si alguien la reutiliza.
    m_impl->liberarProgramas();

    bool primero = true;

    auto compilarSeccion = [&](const std::string &fuente, const char *nombre) -> NSEEL_CODEHANDLE {
        if (fuente.empty()) return nullptr;

        int flags = NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS;
        if (primero) {
            // Limpia la tabla de funciones comunes de una compilacion previa
            // para que no queden funciones del script anterior.
            flags |= NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS_RESET;
            primero = false;
        }

        NSEEL_CODEHANDLE handle =
            NSEEL_code_compile_ex(m_impl->vm, fuente.c_str(), 0, flags);

        if (!handle && outError) {
            const char *detalle = NSEEL_code_getcodeerror(m_impl->vm);
            *outError = std::string(nombre) + ": " +
                        (detalle && *detalle ? detalle : "error de compilacion");
        }
        return handle;
    };

    m_impl->init = compilarSeccion(programas.init, "@init");
    if (!programas.init.empty() && !m_impl->init) {
        m_impl->liberarProgramas();
        return false;
    }

    m_impl->slider = compilarSeccion(programas.slider, "@slider");
    if (!programas.slider.empty() && !m_impl->slider) {
        m_impl->liberarProgramas();
        return false;
    }

    m_impl->block = compilarSeccion(programas.block, "@block");
    if (!programas.block.empty() && !m_impl->block) {
        m_impl->liberarProgramas();
        return false;
    }

    m_impl->sample = compilarSeccion(programas.sample, "@sample");
    if (!programas.sample.empty() && !m_impl->sample) {
        m_impl->liberarProgramas();
        return false;
    }

    m_impl->serialize = compilarSeccion(programas.serialize, "@serialize");
    if (!programas.serialize.empty() && !m_impl->serialize) {
        m_impl->liberarProgramas();
        return false;
    }

    return true;
}

void *NovaJsfxWdlEngine::registrarVariable(const std::string &nombre)
{
    if (!m_impl || !m_impl->vm) return nullptr;
    return NSEEL_VM_regvar(m_impl->vm, nombre.c_str());
}

void NovaJsfxWdlEngine::asignarVariable(void *variable, double valor)
{
    if (variable) *static_cast<EEL_F *>(variable) = valor;
}

double NovaJsfxWdlEngine::leerVariable(const void *variable) const
{
    if (!variable) return 0.0;
    return *static_cast<const EEL_F *>(variable);
}

void NovaJsfxWdlEngine::ejecutarInit()
{
    if (!m_impl || !m_impl->vm) return;
    if (m_impl->init) NSEEL_code_execute(m_impl->init);
    NSEEL_VM_freeRAMIfCodeRequested(m_impl->vm);
}

void NovaJsfxWdlEngine::ejecutarSlider()
{
    if (!m_impl || !m_impl->vm) return;
    if (m_impl->slider) NSEEL_code_execute(m_impl->slider);
    NSEEL_VM_freeRAMIfCodeRequested(m_impl->vm);
}

void NovaJsfxWdlEngine::ejecutarBlock()
{
    if (!m_impl) return;
    if (m_impl->block) NSEEL_code_execute(m_impl->block);
}

void NovaJsfxWdlEngine::ejecutarSample()
{
    if (!m_impl) return;
    if (m_impl->sample) NSEEL_code_execute(m_impl->sample);
}

std::string NovaJsfxWdlEngine::guardarEstado()
{
    if (!m_impl || !m_impl->vm || !m_impl->serialize) return {};

    m_impl->backendEstado.iniciarEscritura();
    NSEEL_code_execute(m_impl->serialize);
    std::string estado = m_impl->backendEstado.finalizar();
    NSEEL_VM_freeRAMIfCodeRequested(m_impl->vm);
    return estado;
}

bool NovaJsfxWdlEngine::cargarEstado(const std::string &estado)
{
    if (!m_impl || !m_impl->vm || !m_impl->serialize || estado.empty()) {
        return false;
    }

    m_impl->backendEstado.iniciarLectura(estado);
    NSEEL_code_execute(m_impl->serialize);
    m_impl->backendEstado.finalizar();
    NSEEL_VM_freeRAMIfCodeRequested(m_impl->vm);
    return true;
}

bool NovaJsfxWdlEngine::tieneSerialize() const
{
    return m_impl && m_impl->serialize;
}

bool NovaJsfxWdlEngine::tieneBlock() const
{
    return m_impl && m_impl->block;
}

bool NovaJsfxWdlEngine::tieneSample() const
{
    return m_impl && m_impl->sample;
}

void NovaJsfxWdlEngine::limpiarMemoria()
{
    if (m_impl && m_impl->vm) NSEEL_VM_freeRAM(m_impl->vm);
}

void NovaJsfxWdlEngine::liberarMemoriaSolicitada()
{
    if (m_impl && m_impl->vm) NSEEL_VM_freeRAMIfCodeRequested(m_impl->vm);
}

void NovaJsfxWdlEngine::setSampleRate(double srate)
{
    if (m_impl && m_impl->srate) *m_impl->srate = srate;
}

void NovaJsfxWdlEngine::setSamplesBlock(int muestras)
{
    if (m_impl && m_impl->samplesblock) *m_impl->samplesblock = muestras;
}

void NovaJsfxWdlEngine::setTempo(double bpm)
{
    if (!m_impl) return;
    if (m_impl->tempo) *m_impl->tempo = bpm;
    if (m_impl->beatPosition && m_impl->playPosition && m_impl->tempo) {
        *m_impl->beatPosition = *m_impl->playPosition * (*m_impl->tempo) / 60.0;
    }
}

void NovaJsfxWdlEngine::setPlayPosition(double segundos)
{
    if (!m_impl) return;
    if (m_impl->playPosition) *m_impl->playPosition = segundos;
    if (m_impl->beatPosition && m_impl->tempo) {
        *m_impl->beatPosition = segundos * (*m_impl->tempo) / 60.0;
    }
}

void NovaJsfxWdlEngine::setNumChannels(int canales)
{
    if (m_impl && m_impl->numChannels) *m_impl->numChannels = canales;
}

void NovaJsfxWdlEngine::setPlayState(int estado)
{
    if (m_impl && m_impl->playState) *m_impl->playState = estado;
}

void NovaJsfxWdlEngine::procesarMuestra(float *izquierda, float *derecha)
{
    if (!m_impl || !m_impl->sample || !m_impl->spl0 || !m_impl->spl1) return;
    if (!izquierda || !derecha) return;

    *m_impl->spl0 = static_cast<EEL_F>(*izquierda);
    *m_impl->spl1 = static_cast<EEL_F>(*derecha);

    NSEEL_code_execute(m_impl->sample);

    *izquierda = static_cast<float>(*m_impl->spl0);
    *derecha = static_cast<float>(*m_impl->spl1);
}
