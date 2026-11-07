#pragma once

#include <memory>
#include <string>

/**
 * Adaptador RAII sobre YSFX (JoepVanlier/ysfx). YSFX es el motor JSFX real
 * que sustituye al anterior adaptador WDL: carga scripts con @gfx, @import,
 * presets y file_*, y procesa audio en bloque.
 *
 * La fachada NovaJsfxEffect y su parser de cabecera siguen siendo propios.
 * Este adaptador solo envuelve la API C de YSFX.
 */
class NovaJsfxYsfxEngine
{
public:
    NovaJsfxYsfxEngine();
    ~NovaJsfxYsfxEngine();

    NovaJsfxYsfxEngine(const NovaJsfxYsfxEngine &) = delete;
    NovaJsfxYsfxEngine &operator=(const NovaJsfxYsfxEngine &) = delete;

    /**
     * Carga y compila el script completo (fuente .jsfx tal cual). Devuelve
     * false y deja el motor sin programa nuevo si falla.
     */
    bool cargar(const std::string &fuente, std::string *outError = nullptr);

    // Variables del host. Solo se usan para los sliders (sliderN); el resto de
    // variables las gestiona YSFX internamente.
    void *registrarVariable(const std::string &nombre);
    void asignarVariable(void *variable, double valor);
    double leerVariable(const void *variable) const;

    void ejecutarInit();
    void ejecutarSlider();
    void ejecutarBlock();   // no-op: YSFX ejecuta @block dentro del proceso
    void ejecutarSample();  // no-op: el proceso se hace en procesarBloque()

    std::string guardarEstado();
    bool cargarEstado(const std::string &estado);

    bool tieneSerialize() const;
    bool tieneBlock() const;
    bool tieneSample() const;

    void limpiarMemoria();
    void liberarMemoriaSolicitada();

    void setSampleRate(double srate);
    void setSamplesBlock(int muestras);
    void setTempo(double bpm);
    void setPlayPosition(double segundos);
    void setNumChannels(int canales);
    void setPlayState(int estado);

    // Procesa un bloque completo de audio (ejecuta @block y @sample).
    void procesarBloque(float *interleavedBuffer, int numFrames, int channels);

    // Compatibilidad con la API anterior (un fotograma).
    void procesarMuestra(float *izquierda, float *derecha);

    // Soporte de la capa gráfica (@gfx)
    bool tieneGfx() const;
    bool obtenerDimensionesGfx(uint32_t &ancho, uint32_t &alto) const;
    void configurarGfx(uint8_t *pixeles, uint32_t ancho, uint32_t alto, uint32_t stride, double factorEscala);
    bool renderizarGfx();
    void actualizarRatonGfx(uint32_t modificadores, int32_t x, int32_t y, uint32_t botones, double ruedaV, double ruedaH);
    void setEstadoVentanaGfx(bool foco, bool visible, bool ratonEncima);
    uint32_t fpsSolicitadosGfx() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
