/**
 * @file TelemetryEventManager.h
 * @brief Gerenciador de distribuição de telemetria baseado em eventos.
 */

#ifndef TELEMETRY_EVENT_MANAGER_H
#define TELEMETRY_EVENT_MANAGER_H

#include <Arduino.h>
#include "TelemetryBuffer.h"

/**
 * Tipo de função callback para receber notificações de telemetria.
 *
 * @param source Identificador da fonte dos dados
 * @param data Buffer de telemetria com os dados
 */
typedef void (*TelemetryEventListener)(const char* source, const TelemetryBuffer& data);

/**
 * @class TelemetryEventManager
 * @brief Gerencia a distribuição de dados de telemetria para múltiplos ouvintes.
 *
 * Implementa um padrão Publisher-Subscriber para desacoplar os componentes
 * que produzem dados de telemetria daqueles que os consomem.
 */
class TelemetryEventManager {
private:
    static const uint8_t MAX_LISTENERS = 5;                ///< Número máximo de ouvintes suportados
    static TelemetryEventListener s_listeners[MAX_LISTENERS]; ///< Array de funções callbacks registradas
    static uint8_t s_listenerCount;                     ///< Contador de ouvintes registrados
    static SemaphoreHandle_t s_mutex;                   ///< Mutex para acesso thread-safe

public:
    /**
     * @brief Inicializa o gerenciador de telemetria.
     *
     * Configura as estruturas de dados internas e cria o mutex de sincronização.
     * Deve ser chamado uma vez durante a inicialização do sistema.
     */
    static void initialize();

    /**
     * @brief Adiciona um ouvinte para receber dados de telemetria.
     *
     * @param listener Função callback a ser invocada quando novos dados estiverem disponíveis
     * @return true se o ouvinte foi adicionado com sucesso, false caso contrário
     */
    static bool addListener(TelemetryEventListener listener);

    /**
     * @brief Remove um ouvinte da lista de notificações.
     *
     * @param listener Função callback a ser removida
     * @return true se o ouvinte foi removido com sucesso, false caso não encontrado
     */
    static bool removeListener(TelemetryEventListener listener);

    /**
     * @brief Distribui dados de telemetria para todos os ouvintes registrados.
     *
     * Este método notifica todos os ouvintes de forma não bloqueante, garantindo
     * que os dados de telemetria sejam disseminados independentemente da
     * limitação de taxa do OutputManager.
     *
     * @param source Identificador da fonte dos dados (ex: nome do módulo)
     * @param data Buffer de telemetria com os dados atuais
     */
    static void distribute(const char* source, const TelemetryBuffer& data);
};

#endif // TELEMETRY_EVENT_MANAGER_H