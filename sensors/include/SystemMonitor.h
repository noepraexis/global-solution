/**
 * @file SystemMonitor.h
 * @brief Monitoramento de recursos do sistema e watchdog.
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "MemoryManager.h"

/**
 * Monitora recursos do sistema e implementa watchdogs para prevenção
 * de travamentos.
 */
class SystemMonitor {
private:
    static SystemMonitor *s_instance;

    // Timestamp da última atualização
    uint32_t m_lastCheckTime;

    // Timestamp do último reset do watchdog
    uint32_t m_lastWatchdogReset;

    // Flag de watchdog ativo
    bool m_watchdogActive;

    // Tempo de boot em ms
    uint32_t m_bootTime;

    // Construtor privado (singleton)
    SystemMonitor();

    /**
     * Função para configurar o watchdog do ESP.
     *
     * @return true se o watchdog foi configurado com sucesso.
     */
    bool setupWatchdog();

public:
    /**
     * Obtém a instância do singleton.
     *
     * @return Referência para a instância única.
     */
    static SystemMonitor &getInstance();

    /**
     * Inicializa o monitor de sistema.
     *
     * @return true se a inicialização foi bem-sucedida.
     */
    bool init();

    /**
     * Atualiza o monitor e reinicia o watchdog.
     * Deve ser chamado periodicamente para evitar reset.
     *
     * @return SystemStats atualizado.
     */
    const SystemStats &update();

    /**
     * Obtém estatísticas atuais do sistema.
     *
     * @return Estatísticas atuais.
     */
    const SystemStats &getStats() const;

    /**
     * Verifica a integridade do sistema.
     *
     * @return true se o sistema está íntegro, false caso contrário.
     */
    bool checkSystemIntegrity();

    /**
     * Reinicia o ESP32 de forma controlada.
     *
     * @param reason Razão do reinício.
     */
    void IRAM_ATTR restart(const char *reason);

    /**
     * Imprime o tempo desde o boot.
     */
    void printUptime();
};

#endif // SYSTEM_MONITOR_H
