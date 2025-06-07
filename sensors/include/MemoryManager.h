/**
 * @file MemoryManager.h
 * @brief Gerenciamento de memória para prevenção de fragmentação.
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "DataTypes.h"

/**
 * Classe para gerenciamento de memória
 *
 * Implementa pools de memória fixos para tipos utilizados frequentemente,
 * reduzindo fragmentação de heap.
 */
class MemoryManager {
private:
    // Singleton
    static MemoryManager *s_instance;

    // Timestamp da última verificação
    uint32_t m_lastCheckTime;

    // Estatísticas de memória
    SystemStats m_stats;

    /**
     * Pool de objetos SensorData
     * Pré-aloca objetos para evitar alocação dinâmica frequente
     */
    struct SensorDataPool {
        static constexpr uint8_t POOL_SIZE = 10;
        SensorData items[POOL_SIZE];
        bool inUse[POOL_SIZE];

        SensorDataPool() {
            // Inicializa todos como livres
            for (uint8_t i = 0; i < POOL_SIZE; i++) {
                inUse[i] = false;
            }
        }
    };

    // Pools de objetos
    SensorDataPool m_sensorDataPool;

    // Buffer JSON estático para evitar alocação dinâmica
    char m_jsonBuffer[JSON_BUFFER_SIZE];
    bool m_jsonBufferInUse;

    // Construtor privado (singleton)
    MemoryManager();

public:
    /**
     * Obtém a instância do singleton.
     *
     * @return Referência para a instância única.
     */
    static MemoryManager &getInstance();

    /**
     * Inicializa o gerenciador de memória.
     *
     * @return true se a inicialização foi bem-sucedida.
     */
    bool init();

    /**
     * Adquire um objeto SensorData do pool.
     *
     * @return Ponteiro para o objeto, ou nullptr se o pool estiver cheio.
     */
    SensorData *acquireSensorData();

    /**
     * Libera um objeto SensorData de volta para o pool.
     *
     * @param data Ponteiro para o objeto a ser liberado.
     * @return true se o objeto foi liberado, false caso contrário.
     */
    bool releaseSensorData(SensorData *data);

    /**
     * Adquire o buffer JSON.
     *
     * @return Ponteiro para o buffer, ou nullptr se estiver em uso.
     */
    char *acquireJsonBuffer();

    /**
     * Libera o buffer JSON.
     *
     * @return true se o buffer foi liberado, false caso contrário.
     */
    bool releaseJsonBuffer();

    /**
     * Atualiza as estatísticas de memória.
     *
     * @return Estatísticas atualizadas.
     */
    const SystemStats &updateStats();

    /**
     * Obtém as estatísticas atuais sem atualizar.
     *
     * @return Estatísticas atuais.
     */
    const SystemStats &getStats() const;

    /**
     * Verifica integridade da memória.
     *
     * @return true se a memória está íntegra, false caso contrário.
     */
    bool checkMemoryIntegrity();

    /**
     * Imprime estatísticas de memória no console (modo debug).
     */
    void printStats();
};

#endif // MEMORY_MANAGER_H
