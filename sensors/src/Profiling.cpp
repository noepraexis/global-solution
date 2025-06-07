/**
 * @file Profiling.cpp
 * @brief Implementa funções para instrumentação e análise de desempenho de código.
 */

#include <Arduino.h>
#include "Config.h"
#include "Profiling.h"

namespace Profiling {
    // Variáveis para medir o tempo de execução de funções
    static uint32_t function_entry_time = 0;
    static uint32_t total_execution_time = 0;
    static uint32_t function_calls = 0;

    /**
     * Obtém as estatísticas de profiling acumuladas.
     *
     * @param total_time Ponteiro para armazenar o tempo total de execução.
     * @param calls Ponteiro para armazenar o número de chamadas de função.
     */
    void getStats(uint32_t *total_time, uint32_t *calls) {
        if (total_time) {
            *total_time = total_execution_time;
        }
        if (calls) {
            *calls = function_calls;
        }
    }

    /**
     * Reinicia as estatísticas de profiling.
     */
    void reset() {
        total_execution_time = 0;
        function_calls = 0;
    }
}

extern "C" {
    /**
     * Função chamada automaticamente ao entrar em uma função instrumentada.
     * Registra timestamp de entrada para profiling de desempenho.
     *
     * @param this_fn Ponteiro para a função sendo acessada.
     * @param call_site Ponteiro para o local da chamada.
     */
    void __cyg_profile_func_enter(void *this_fn, void *call_site) {
        // Registra tempo de entrada
        Profiling::function_entry_time = micros();
        Profiling::function_calls++;

        // No modo debug, pode imprimir endereços de função
        if (DEBUG_MODE) {
            APP_DEBUG_PRINT("PROFILE: Enter function at address %p called from %p\n",
                         this_fn, call_site);
        }
    }

    /**
     * Função chamada automaticamente ao sair de uma função instrumentada.
     * Calcula tempo de execução para profiling de desempenho.
     *
     * @param this_fn Ponteiro para a função sendo finalizada.
     * @param call_site Ponteiro para o local da chamada.
     */
    void __cyg_profile_func_exit(void *this_fn, void *call_site) {
        uint32_t now = micros();
        uint32_t execution_time = now - Profiling::function_entry_time;
        Profiling::total_execution_time += execution_time;

        // Opcional: imprime tempo de execução no modo debug
        if (DEBUG_MODE) {
            APP_DEBUG_PRINT("PROFILE: Exit function at address %p - execution time: %u us\n",
                         this_fn, execution_time);
        }
    }
}
