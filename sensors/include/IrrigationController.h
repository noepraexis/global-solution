/**
 * @file IrrigationController.h
 * @brief Controlador inteligente do sistema de irrigação.
 */

#ifndef IRRIGATION_CONTROLLER_H
#define IRRIGATION_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"
#include "DataTypes.h"
#include "Hardware.h"

/**
 * @struct IrrigationData
 * @brief Estrutura para armazenamento dos dados do sistema de irrigação.
 */
struct IrrigationData {
    bool pumpActive;              ///< Estado atual da bomba
    uint32_t activationTime;      ///< Timestamp da ativação atual
    uint32_t lastDeactivationTime; ///< Timestamp da última desativação
    uint32_t totalRuntime;        ///< Tempo total de funcionamento (segundos)
    uint8_t dailyActivations;     ///< Contador de ativações no dia
    float currentThreshold;       ///< Limiar atual de umidade
    bool manualMode;              ///< Modo manual ativo
    bool emergencyShutdown;       ///< Estado de emergência
    uint32_t lastDecisionTime;    ///< Timestamp da última decisão

    // Construtor com valores padrão
    IrrigationData() : pumpActive(false),
                      activationTime(0),
                      lastDeactivationTime(0),
                      totalRuntime(0),
                      dailyActivations(0),
                      currentThreshold(MOISTURE_THRESHOLD_LOW),
                      manualMode(false),
                      emergencyShutdown(false),
                      lastDecisionTime(0) {}
};

/**
 * @class IrrigationController
 * @brief Controlador principal do sistema de irrigação com lógica inteligente.
 *
 * Implementa controle automático baseado em dados de sensores, com proteções
 * de segurança, modo manual e sistema de fail-safe robusto seguindo os padrões
 * arquiteturais do projeto.
 */
class IrrigationController {
public:
    /**
     * @brief Obtém a instância única do controlador.
     * @return Referência à instância singleton.
     */
    static IrrigationController& getInstance();

    /**
     * @brief Inicializa o controlador de irrigação.
     * @return true se a inicialização foi bem-sucedida.
     */
    bool init();

    /**
     * @brief Atualiza o sistema de irrigação.
     *
     * Deve ser chamado periodicamente para verificar condições
     * e aplicar lógica de controle.
     *
     * @return true se houve mudança de estado.
     */
    bool update();

    /**
     * @brief Atualiza a decisão de irrigação baseada nos dados dos sensores.
     *
     * @param sensorData Dados atuais dos sensores.
     * @return true se houve mudança na decisão.
     */
    bool updateDecision(const SensorData& sensorData);

    /**
     * @brief Ativa a irrigação manualmente.
     *
     * @param duration Duração em milissegundos (0 = até desligar manualmente).
     * @return true se a ativação foi bem-sucedida.
     */
    bool activateManual(uint32_t duration = 0);

    /**
     * @brief Desativa a irrigação.
     *
     * @param manual true se foi desativação manual.
     * @return true se a desativação foi bem-sucedida.
     */
    bool deactivate(bool manual = false);

    /**
     * @brief Verifica se a irrigação está ativa.
     *
     * @return true se a bomba está ligada.
     */
    bool isActive() const;

    /**
     * @brief Obtém os dados atuais do sistema de irrigação.
     *
     * @return Referência constante aos dados de irrigação.
     */
    const IrrigationData& getData() const;

    /**
     * @brief Obtém o tempo total de funcionamento em segundos.
     *
     * @return Tempo total de funcionamento.
     */
    uint32_t getTotalRuntime() const;

    /**
     * @brief Obtém o timestamp da última ativação.
     *
     * @return Timestamp da última ativação.
     */
    uint32_t getLastActivation() const;

    /**
     * @brief Processa comando recebido via WebSocket.
     *
     * @param activate true para ativar, false para desativar.
     * @param duration Duração em milissegundos (apenas para ativação).
     * @return true se o comando foi processado com sucesso.
     */
    bool processCommand(bool activate, uint32_t duration = 0);

    /**
     * @brief Ativa o shutdown de emergência.
     *
     * Para a irrigação imediatamente e bloqueia futuras ativações.
     */
    void emergencyShutdown();

    /**
     * @brief Reseta o shutdown de emergência.
     *
     * @return true se o reset foi bem-sucedido.
     */
    bool resetEmergency();

    /**
     * @brief Verifica se o sistema está inicializado.
     *
     * @return true se inicializado corretamente.
     */
    bool isInitialized() const;

private:
    /**
     * @brief Construtor privado (padrão Singleton).
     */
    IrrigationController();

    /**
     * @brief Destrutor privado.
     */
    ~IrrigationController();

    // Impede cópia e atribuição
    IrrigationController(const IrrigationController&) = delete;
    IrrigationController& operator=(const IrrigationController&) = delete;

    /**
     * @brief Ativa a irrigação interna.
     *
     * @param duration Duração em milissegundos.
     * @param manual true se ativação manual.
     * @return true se ativação foi bem-sucedida.
     */
    bool activateInternal(uint32_t duration, bool manual);

    /**
     * @brief Verifica condições de segurança.
     *
     * @return true se é seguro operar.
     */
    bool checkSafetyConditions();

    /**
     * @brief Verifica timeouts e limites de tempo.
     *
     * @return true se houve mudança de estado.
     */
    bool checkTimeouts();

    /**
     * @brief Atualiza estatísticas de runtime.
     */
    void updateRuntime();

    /**
     * @brief Reseta contador diário (chamado uma vez por dia).
     */
    void resetDailyCounters();

    // Dados do sistema
    IrrigationData m_data;              ///< Dados principais
    bool m_initialized;                 ///< Flag de inicialização
    uint32_t m_scheduledStopTime;       ///< Tempo programado para parar
    uint32_t m_lastRuntimeUpdate;       ///< Última atualização de runtime
    uint32_t m_lastDayReset;            ///< Último reset diário

    // Instância singleton
    static IrrigationController* s_instance;
};

#endif // IRRIGATION_CONTROLLER_H