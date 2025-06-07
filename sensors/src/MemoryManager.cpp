/**
 * @file MemoryManager.cpp
 * @brief Implementação do gerenciador de memória.
 */

#include "MemoryManager.h"
#include "LogSystem.h"
#include <esp_heap_caps.h>

// Nome do módulo para logs
#define MODULE_NAME "Memory"

// Inicializa o ponteiro da instância singleton como null
MemoryManager *MemoryManager::s_instance = nullptr;

MemoryManager::MemoryManager()
    : m_lastCheckTime(0), m_jsonBufferInUse(false) {
    // Inicializa as estatísticas de memória
    updateStats();

    // Inicialização silenciosa no construtor, já que o report principal
    // será feito no init()
}

MemoryManager &MemoryManager::getInstance() {
    // Se a instância ainda não foi criada, cria-a
    if (s_instance == nullptr) {
        s_instance = new MemoryManager();
    }
    return *s_instance;
}

bool MemoryManager::init() {
    LOG_INFO(MODULE_NAME, "Inicializando Gerenciador de Memória");

    // Inicialização das estruturas de memória
    updateStats(); // Atualiza estatísticas iniciais

    // Limpa o buffer JSON
    memset(m_jsonBuffer, 0, JSON_BUFFER_SIZE);
    m_jsonBufferInUse = false;

    // Inicializa os pools
    for (uint8_t i = 0; i < SensorDataPool::POOL_SIZE; i++) {
        m_sensorDataPool.inUse[i] = false;
    }

    LOG_INFO(MODULE_NAME, "Pool de SensorData: %u slots configurados",
                 SensorDataPool::POOL_SIZE);
    LOG_INFO(MODULE_NAME, "Buffer JSON: %u bytes alocados",
                 JSON_BUFFER_SIZE);
    LOG_INFO(MODULE_NAME, "Heap livre inicial: %u bytes",
                 m_stats.freeHeap);

    return true;
}

SensorData *MemoryManager::acquireSensorData() {
    // Procura um slot livre no pool
    for (uint8_t i = 0; i < SensorDataPool::POOL_SIZE; i++) {
        if (!m_sensorDataPool.inUse[i]) {
            m_sensorDataPool.inUse[i] = true;
            // Reseta o objeto antes de retorná-lo
            m_sensorDataPool.items[i] = SensorData();

            if (DEBUG_MEMORY) {
                LOG_DEBUG(MODULE_NAME, "SensorData adquirido (slot %u)", i);
            }
            return &m_sensorDataPool.items[i];
        }
    }

    // Se não encontrou slot livre, retorna nullptr
    LOG_ERROR(MODULE_NAME, "Pool de SensorData esgotado");
    return nullptr;
}

bool MemoryManager::releaseSensorData(SensorData *data) {
    // Verifica se o ponteiro é válido e pertence ao pool
    if (data == nullptr) return false;

    for (uint8_t i = 0; i < SensorDataPool::POOL_SIZE; i++) {
        if (data == &m_sensorDataPool.items[i]) {
            m_sensorDataPool.inUse[i] = false;
            if (DEBUG_MEMORY) {
                LOG_DEBUG(MODULE_NAME, "SensorData liberado (slot %u)", i);
            }
            return true;
        }
    }

    // Se chegou aqui, o ponteiro não pertence ao pool
    LOG_ERROR(MODULE_NAME, "SensorData não pertence ao pool");
    return false;
}

char* MemoryManager::acquireJsonBuffer() {
    if (m_jsonBufferInUse) {
        LOG_WARN(MODULE_NAME, "Buffer JSON já em uso");
        return nullptr;
    }

    m_jsonBufferInUse = true;
    // Limpa o buffer antes de retorná-lo
    memset(m_jsonBuffer, 0, JSON_BUFFER_SIZE);

    if (DEBUG_MEMORY) {
        LOG_DEBUG(MODULE_NAME, "Buffer JSON adquirido");
    }
    return m_jsonBuffer;
}

bool MemoryManager::releaseJsonBuffer() {
    if (!m_jsonBufferInUse) {
        LOG_WARN(MODULE_NAME, "Buffer JSON já está livre");
        return false;
    }

    m_jsonBufferInUse = false;
    if (DEBUG_MEMORY) {
        LOG_DEBUG(MODULE_NAME, "Buffer JSON liberado");
    }
    return true;
}

const SystemStats &MemoryManager::updateStats() {
    // Atualiza estatísticas apenas a cada segundo para evitar sobrecarga
    uint32_t currentTime = millis();
    if (currentTime - m_lastCheckTime < 1000 && m_lastCheckTime > 0) {
        return m_stats;
    }

    m_lastCheckTime = currentTime;

    // Atualiza estatísticas de heap
    m_stats.freeHeap = esp_get_free_heap_size();
    m_stats.minFreeHeap = esp_get_minimum_free_heap_size();

    // Calcula fragmentação aproximada
    uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (m_stats.freeHeap > 0) {
        // Fragmentação = 100 - (maior bloco livre / heap total livre * 100)
        m_stats.heapFragmentation = 100 - (largestBlock * 100 / m_stats.freeHeap);
    } else {
        m_stats.heapFragmentation = 0;
    }

    // Atualiza tempo de atividade (converte de ms para s)
    m_stats.uptime = currentTime / 1000;

    // Carga da CPU não é facilmente obtida no ESP32 sem uso de FreeRTOS
    // avançado, então deixamos em 0 por enquanto
    m_stats.cpuLoad = 0;

    return m_stats;
}

const SystemStats &MemoryManager::getStats() const {
    return m_stats;
}

bool MemoryManager::checkMemoryIntegrity() {
    // Verifica integridade do heap
    bool integrity = heap_caps_check_integrity_all(true);

    if (!integrity) {
        LOG_ERROR(MODULE_NAME, "ALERTA: Corrupção de heap detectada!");
    }

    return integrity;
}

void MemoryManager::printStats() {
    if (!DEBUG_MEMORY) return;

    // As estatísticas agora são exibidas no rotator de mensagens
    // do SensorManager, reduzindo a verbosidade do console

    // A cada 30 segundos (ou múltiplo disso) exibimos um relatório completo
    static uint32_t lastFullReport = 0;
    updateStats();

    if (millis() - lastFullReport >= 30000) {
        LOG_INFO(MODULE_NAME, "=== Relatório de Memória ===");
        LOG_INFO(MODULE_NAME, "Heap livre: %u bytes", m_stats.freeHeap);
        LOG_INFO(MODULE_NAME, "Heap livre mínimo: %u bytes", m_stats.minFreeHeap);
        LOG_INFO(MODULE_NAME, "Fragmentação: %u%%", m_stats.heapFragmentation);
        LOG_INFO(MODULE_NAME, "Maior bloco livre: %u bytes",
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        LOG_INFO(MODULE_NAME, "Tempo de atividade: %u segundos", m_stats.uptime);

        lastFullReport = millis();
    }
}
