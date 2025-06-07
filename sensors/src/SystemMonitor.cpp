/**
 * @file SystemMonitor.cpp
 * @brief Implementação do monitor de sistema.
 */

#include "SystemMonitor.h"
#include "LogSystem.h"

// Define o nome do módulo para logging
#define MODULE_NAME "SysMonitor"

// Inicializa o ponteiro da instância singleton como null
SystemMonitor *SystemMonitor::s_instance = nullptr;

SystemMonitor::SystemMonitor()
    : m_lastCheckTime(0),
    m_lastWatchdogReset(0),
    m_watchdogActive(false),
    m_bootTime(millis()) {
}

SystemMonitor &SystemMonitor::getInstance() {
    // Se a instância ainda não foi criada, cria-a
    if (s_instance == nullptr) {
        s_instance = new SystemMonitor();
    }
    return *s_instance;
}

bool SystemMonitor::init() {
    LOG_INFO(MODULE_NAME, "Inicializando Monitor do Sistema");

    // Inicializa o watchdog se configurado
    if (ENABLE_TASK_WATCHDOG) {
        m_watchdogActive = setupWatchdog();

        LOG_INFO(MODULE_NAME, "Watchdog %s",
                m_watchdogActive ? "ativado" : "falhou ao ativar");
    } else {
        LOG_INFO(MODULE_NAME, "Watchdog desativado por configuração");
    }

    LOG_INFO(MODULE_NAME, "Monitor do sistema inicializado com sucesso");
    return true;
}

bool SystemMonitor::setupWatchdog() {
    esp_err_t err;

    // Watchdog já tem seus padrões de bloqueio configurados no LogSystem
    // (não precisamos mais adicionar aqui, estã no construtor do LogRouter)

    // Inicializa o Task Watchdog Timer (TWDT)
    // Modificamos para desativar o panic automático (segundo parametro = false)
    // para permitir nosso tratamento personalizado e evitar mensagens no console
    err = esp_task_wdt_init(WATCHDOG_TIMEOUT / 1000, false);
    if (err != ESP_OK) {
        LOG_ERROR(MODULE_NAME, "Erro ao inicializar TWDT: %d", err);
        return false;
    }

    // Adiciona a tarefa atual ao watchdog
    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
        LOG_ERROR(MODULE_NAME, "Erro ao adicionar tarefa ao TWDT: %d", err);
        return false;
    }

    // Registra timestamp inicial para controle de resets
    m_lastWatchdogReset = millis();

    LOG_INFO(MODULE_NAME, "Watchdog configurado com timeout de %u ms", WATCHDOG_TIMEOUT);

    return true;
}

const SystemStats &SystemMonitor::update() {
    // Atualiza estatísticas a cada 1 segundo
    uint32_t currentTime = millis();

    // Reinicia o watchdog se ativo - isso é um processo normal
    // A lógica foi refinada para ser mais robusta e sem mensagens intrusivas
    if (m_watchdogActive) {
        uint32_t timeElapsed = currentTime - m_lastWatchdogReset;

        // Redefine o tempo máximo para 75% do timeout para garantir margem de segurança
        if (timeElapsed >= (WATCHDOG_TIMEOUT * 3 / 4)) {
            // Reinicia o watchdog com tempo adequado
            esp_task_wdt_reset();
            m_lastWatchdogReset = currentTime;

            // Se precisar mostrar estatísticas de watchdog em modo de debug,
            // usa o sistema de telemetria para não interferir com outras saídas
            #ifdef DEBUG_WATCHDOG
            #if DEBUG_WATCHDOG
            static uint32_t watchdogResets = 0;
            static uint32_t telemetryToken = 0;

            // Obtém token de telemetria na primeira vez ou reusa o existente
            if (telemetryToken == 0) {
                telemetryToken = TELEMETRY_BEGIN("Watchdog");
            }

            // Atualiza a telemetria
            if (telemetryToken != 0) {
                TELEMETRY_UPDATE(telemetryToken,
                    "Watchdog: %u resets | Último intervalo: %u ms",
                    ++watchdogResets, timeElapsed);
            }
            #endif
            #endif
        }
    }

    // Atualiza as estatísticas de memória se for hora
    if (currentTime - m_lastCheckTime >= 1000) {
        m_lastCheckTime = currentTime;
        MemoryManager::getInstance().updateStats();

        // Verifica integridade da memória a cada 10 segundos
        if ((currentTime / 1000) % 10 == 0) {
            checkSystemIntegrity();
        }
    }

    return MemoryManager::getInstance().getStats();
}

const SystemStats &SystemMonitor::getStats() const {
    return MemoryManager::getInstance().getStats();
}

bool SystemMonitor::checkSystemIntegrity() {
    bool memoryOk = MemoryManager::getInstance().checkMemoryIntegrity();

    if (!memoryOk) {
        LOG_FATAL(MODULE_NAME, "ALERTA DE INTEGRIDADE: Detectada corrupção de memória!");

        // Se estiver em modo de depuração, apenas notifica
        // Em produção, reinicia o sistema
        if (!DEBUG_MODE) {
            restart("Heap corrupto");
        }
    }

    return memoryOk;
}

void IRAM_ATTR SystemMonitor::restart(const char *reason) {
    // Código colocado na IRAM para garantir execução mesmo com heap corrompido

    // Desativa o watchdog antes de reiniciar
    if (m_watchdogActive) {
        esp_task_wdt_delete(NULL);
    }

    // Primeiro tentamos usar o sistema de log para registrar o erro
    // O nível FATAL garante que será registrado no buffer circular
    LOG_FATAL(MODULE_NAME, "*** REINICIALIZAÇÃO DE EMERGÊNCIA ***");
    LOG_FATAL(MODULE_NAME, "Razão: %s", reason);
    LOG_FATAL(MODULE_NAME, "Reiniciando ESP32...");

    // Em situação de reinicialização de emergência, também usamos Serial diretamente
    // como backup, pois o sistema de log pode depender de heap que pode estar corrompido

    // Forçamos quebras de linha para evitar conflito com atualizações na mesma linha
    Serial.println();
    Serial.println();
    delay(20);

    // Formatação mais clara com separadores visuais
    Serial.println("======================================");
    delay(10);
    Serial.println("*** REINICIALIZAÇÃO DE EMERGÊNCIA ***");
    delay(10);

    // Formata manualmente em vez de usar printf para maior controle
    Serial.print("Razão: ");
    Serial.println(reason);
    delay(10);

    Serial.println("Reiniciando ESP32...");
    Serial.println("======================================");

    // Pequeno delay para permitir que as mensagens sejam enviadas
    delay(100);

    // Reinicia o ESP32
    ESP.restart();
}

void SystemMonitor::printUptime() {
    // Método mantido vazio para compatibilidade com código existente
    // As informações de uptime e stats são mostradas diretamente no
    // rotator de mensagens do SensorManager
}
