#pragma once

#include "core/NovaPeakData.h"
#include "audio/NovaDecodedAudio.h"

/**
 * Calcula los picos de un audio ya decodificado.
 *
 * Devuelve los dos niveles que usa el renderizador de la forma de onda: el
 * detalle (muchos puntos, para el zoom máximo) y la vista general (pocos
 * puntos, para el zoom out). Cada punto trae el mínimo, el máximo y el rms
 * de su trozo de audio.
 *
 * No depende de Qt ni de la plataforma: se puede probar en el escritorio
 * (ver tests/test_audio_peaks.cpp).
 *
 * @param audio          Audio decodificado (muestras intercaladas).
 * @param detailPoints   Número de puntos del nivel de detalle.
 * @param overviewPoints Número de puntos de la vista general.
 * @return Los picos calculados (vaciados si el audio no tiene muestras).
 */
PeakData buildPeakData(const NovaDecodedAudio& audio, int detailPoints, int overviewPoints);
