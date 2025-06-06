/**
 * @file TelemetryEventManager.cpp
 * @brief Implementação do gerenciador de distribuição de telemetria.
 */

#include "TelemetryEventManager.h"
#include "LogSystem.h"

// Define o nome do módulo para logging
#define MODULE_NAME "TelemetryEvt"

// Inicialização das variáveis estáticas
TelemetryEventListener TelemetryEventManager::s_listeners[MAX_LISTENERS] = {nullptr};
uint8_t TelemetryEventManager::s_listenerCount = 0;
SemaphoreHandle_t TelemetryEventManager::s_mutex = nullptr;

void TelemetryEventManager::initialize() {
    // Cria o mutex apenas uma vez
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == nullptr) {
            LOG_ERROR(MODULE_NAME, "Falha ao criar mutex para gerenciador de telemetria");
        }
    }
}

bool TelemetryEventManager::addListener(TelemetryEventListener listener) {
    if (listener == nullptr) return false;

    // Sempre garantir inicialização antes de tentar usar o mutex
    if (s_mutex == nullptr) {
        initialize();
    }

    // Se falhar na inicialização, tenta uma última vez
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == nullptr) return false;
    }

    bool result = false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_listenerCount < MAX_LISTENERS) {
            // Verificar se o listener já está registrado para evitar duplicação
            bool found = false;
            for (uint8_t i = 0; i < s_listenerCount; i++) {
                if (s_listeners[i] == listener) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                s_listeners[s_listenerCount++] = listener;

                // Garantir que a modificação seja visível para outras tarefas
                // (barreira de memória implícita ao liberar o mutex)
            }
            result = true;
        }
        xSemaphoreGive(s_mutex);
    }
    return result;
}

bool TelemetryEventManager::removeListener(TelemetryEventListener listener) {
    if (listener == nullptr) return false;

    // Garantir inicialização do mutex para evitar crashes
    if (s_mutex == nullptr) {
        initialize();
        if (s_mutex == nullptr) return false;
    }

    bool result = false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < s_listenerCount; i++) {
            if (s_listeners[i] == listener) {
                // Move os ouvintes restantes para preencher o espaço
                for (uint8_t j = i; j < s_listenerCount - 1; j++) {
                    s_listeners[j] = s_listeners[j + 1];
                }
                s_listeners[s_listenerCount - 1] = nullptr; // Garante limpeza do último elemento
                s_listenerCount--;
                result = true;
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return result;
}

void TelemetryEventManager::distribute(const char* source, const TelemetryBuffer& data) {
    // Log para debug de distrição de telemetria
    LOG_DEBUG(MODULE_NAME, "Distribuindo telemetria de [%s]: %.1f°C, %.1f%%",
              source, data.temperature, data.humidity);

    // Verificação rápida para otimização - se não há ouvintes, retorna imediatamente
    if (s_listenerCount == 0) {
        LOG_DEBUG(MODULE_NAME, "Sem listeners registrados para telemetria");
        // Se o sistema não está inicializado, tenta inicializar para futuros listeners
        if (s_mutex == nullptr) {
            initialize();
        }
        return;
    }

    // Garantir que temos mutex válido para acessar a lista de ouvintes
    if (s_mutex == nullptr) {
        initialize();
        if (s_mutex == nullptr) return; // Falha na inicialização, não pode prosseguir
    }

    // Uso de buffer na stack para minimizar overhead de memória
    TelemetryEventListener listeners[MAX_LISTENERS];
    uint8_t count = 0;

    // Seção crítica curta: copia referências dos listeners rapidamente
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) { // Timeout curto para não bloquear
        count = s_listenerCount;
        LOG_DEBUG(MODULE_NAME, "Notificando %d listeners sobre telemetria", count);
        for (uint8_t i = 0; i < count && i < MAX_LISTENERS; i++) {
            listeners[i] = s_listeners[i];
        }
        xSemaphoreGive(s_mutex);
    }

    // Fora da seção crítica: invoca cada listener com os dados
    for (uint8_t i = 0; i < count; i++) {
        if (listeners[i] != nullptr) {
            // Invocação direta, sem verificações adicionais para maximizar velocidade
            listeners[i](source, data);
        }
    }
}