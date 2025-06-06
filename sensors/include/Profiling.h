/**
 * @file Profiling.h
 * @brief Define funções para instrumentação e análise de desempenho de código.
 */

#ifndef PROFILING_H
#define PROFILING_H

#include <Arduino.h>
#include "Config.h"

namespace Profiling {
    /**
     * Obtém as estatísticas de profiling acumuladas.
     *
     * @param total_time Ponteiro para armazenar o tempo total de execução.
     * @param calls Ponteiro para armazenar o número de chamadas de função.
     */
    void getStats(uint32_t *total_time, uint32_t *calls);

    /**
     * Reinicia as estatísticas de profiling.
     */
    void reset();
}

#endif // PROFILING_H