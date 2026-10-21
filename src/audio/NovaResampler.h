#pragma once

#include <QtGlobal>

#include <vector>

/**
 * Remuestreador lineal entre dos tasas de muestreo.
 *
 * Mantiene una posición fraccionaria persistente entre llamadas: la posición
 * dentro del audio no se pierde al cambiar de bloque, de modo que el audio
 * no tiene discontinuidades (clics) entre bloques. Es lo que necesita el
 * callback de Oboe, que pide el audio en bloques de tamaño fijo.
 *
 * La entrada y la salida son muestras intercaladas (L R L R ...). El número
 * de canales se fija en `setRates()` para que la interpolación se haga
 * dentro de cada canal: si no, los canales se irían desplazando uno
 * respecto del otro al remuestrear.
 *
 * Si la tasa de entrada y la de salida son iguales, el remuestreador hace
 * una copia exacta de las muestras.
 *
 * **Invariante:** el índice de la primera muestra pendiente nunca pasa del
 * final del buffer. Con razones de remuestreo altas (un fichero de 192 kHz
 * con una salida de 48 kHz) la posición puede avanzar más allá de la última
 * muestra disponible, así que lo consumido se acota a lo que hay: si no, la
 * compactación borraría muestras de más y se saldría del buffer.
 */
class NovaResampler {
public:
    /**
     * Fija las tasas de entrada y de salida y el número de canales.
     *
     * Cambiar las tasas invalida lo que hubiera en el buffer, así que el
     * remuestreador queda como recién construido.
     *
     * **Atención al número de canales:** el valor por defecto (1) es para
     * audio mono. Con audio estéreo hay que indicar cuántos canales hay: si
     * se deja el valor por defecto, la interpolación se haría entre muestras
     * de canales distintos y el audio saldría corrupto.
     *
     * @param inRate   Tasa del audio de entrada, en Hz.
     * @param outRate  Tasa del audio de salida, en Hz.
     * @param channels Número de canales intercalados (1 si no se indica).
     */
    void setRates(int inRate, int outRate, int channels = 1);

    /** Vacia el buffer y vuelve al principio del audio. */
    void reset();

    /**
     * Añade `inFrames` fotogramas intercalados de entrada al buffer y
     * convierte en fotogramas de salida todo lo que pueda, hasta
     * `outCapacity`.
     *
     * Lo que sobra de la entrada se queda en el buffer para la siguiente
     * llamada, de modo que la posición fraccionaria se conserva.
     *
     * El puntero de entrada no puede apuntar dentro del buffer del propio
     * remuestreador.
     *
     * @param in          Muestras intercaladas de entrada (puede ser nullptr).
     * @param inFrames    Número de fotogramas de entrada que se añaden.
     * @param out         Buffer de salida (intercalado).
     * @param outCapacity Número máximo de fotogramas que caben en `out`.
     * @return Número de fotogramas escritos en `out` (0 si no se pudo escribir
     *         ninguno).
     */
    int process(const float* in, int inFrames, float* out, int outCapacity);

private:
    /** Saca del buffer las muestras ya convertidas. */
    void compactar();

    int m_tasaEntrada = 0;
    int m_tasaSalida = 0;
    int m_canales = 1;

    /** Fotogramas de entrada por cada fotograma de salida. */
    double m_ratio = 1.0;

    /** Muestras intercaladas pendientes de convertir. */
    std::vector<float> m_buffer;

    /** Índice de la primera muestra pendiente dentro de `m_buffer`. */
    size_t m_inicio = 0;

    /** Posición (en fotogramas) de la próxima salida dentro del buffer. */
    double m_pos = 0.0;
};
