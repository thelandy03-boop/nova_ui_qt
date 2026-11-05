#pragma once

#include <memory>
#include <string>

/**
 * Adaptador RAII sobre el motor EEL2 real de Cockos (WDL).
 *
 * Nova Studio mantiene su propia fachada (NovaJsfxEffect) y su parser de
 * cabecera (desc:, sliderN:). Este adaptador solo se encarga de lo que hace
 * el motor de verdad: crear una VM EEL2, compilar las secciones @init,
 * @slider, @block, @sample y @serialize, y ejecutarlas con las variables del
 * host.
 *
 * La cabecera no incluye ningun fichero de WDL: los tipos C del motor
 * (NSEEL_VMCTX, NSEEL_CODEHANDLE, EEL_F) quedan encerrados en el .cpp.
 */
class NovaJsfxWdlEngine
{
public:
    struct Programas {
        std::string init;
        std::string slider;
        std::string block;
        std::string sample;
        std::string serialize;
    };

    NovaJsfxWdlEngine();
    ~NovaJsfxWdlEngine();

    NovaJsfxWdlEngine(const NovaJsfxWdlEngine &) = delete;
    NovaJsfxWdlEngine &operator=(const NovaJsfxWdlEngine &) = delete;

    /**
     * Compila las cinco secciones. Las secciones vacias no generan codigo.
     * Devuelve false y deja el motor sin programas nuevos si alguna falla.
     */
    bool compilar(const Programas &programas, std::string *outError = nullptr);

    // Variables del host. El puntero devuelto pertenece a la VM.
    void *registrarVariable(const std::string &nombre);
    void asignarVariable(void *variable, double valor);
    double leerVariable(const void *variable) const;

    void ejecutarInit();
    void ejecutarSlider();
    void ejecutarBlock();
    void ejecutarSample();

    /**
     * Ejecuta @serialize en modo escritura y devuelve el estado interno.
     * Los sliders no van aqui: se persisten aparte, como en REAPER.
     * Devuelve una cadena vacia si el script no tiene @serialize.
     */
    std::string guardarEstado();

    /**
     * Ejecuta @serialize en modo lectura sobre el estado dado.
     * Devuelve false si el script no tiene @serialize o el estado esta vacio.
     */
    bool cargarEstado(const std::string &estado);

    bool tieneSerialize() const;

    bool tieneBlock() const;
    bool tieneSample() const;

    // Vacía la memoria RAM de los scripts (delay buffers, etc.).
    void limpiarMemoria();

    // Respeta freembuf()/freemem() si el script los pidio en @init o @slider.
    void liberarMemoriaSolicitada();

    void setSampleRate(double srate);
    void setSamplesBlock(int muestras);
    void setTempo(double bpm);
    void setPlayPosition(double segundos);
    void setNumChannels(int canales);
    void setPlayState(int estado);

    /**
     * Escribe spl0/spl1, ejecuta @sample y devuelve el resultado.
     * izquierda y derecha pueden apuntar al mismo sitio (mono).
     */
    void procesarMuestra(float *izquierda, float *derecha);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
