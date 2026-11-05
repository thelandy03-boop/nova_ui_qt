#include "audio/dsp/jsfx/NovaJsfxWdlEngine.h"

#include <cmath>
#include <mutex>
#include <string>

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
