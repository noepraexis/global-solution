/**
 * @file IrrigationController.cpp
 * @brief Implementação do controlador inteligente do sistema de irrigação.
 */

#include "IrrigationController.h"
#include "LogSystem.h"

// Define o nome do módulo para logging
#define MODULE_NAME "IrrigationController"

// Inicialização da variável estática
IrrigationController* IrrigationController::s_instance = nullptr;

IrrigationController::IrrigationController()
    : m_initialized(false),
      m_scheduledStopTime(0),
      m_lastRuntimeUpdate(0),
      m_lastDayReset(0) {
}

IrrigationController::~IrrigationController() {
    // Garante que a bomba seja desligada ao destruir
    if (m_data.pumpActive) {
        Hardware::setRelayState(Hardware::RELAY_OFF);
        LOG_INFO(MODULE_NAME, "Bomba desligada no destrutor");
    }
}

IrrigationController& IrrigationController::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new IrrigationController();
    }
    return *s_instance;
}

bool IrrigationController::init() {
    // Contador estático para diagnóstico
    static uint8_t initCallCount = 0;
    initCallCount++;

    // Proteção contra dupla inicialização
    if (m_initialized) {
        LOG_WARN(MODULE_NAME, "Controlador já inicializado - operação ignorada (chamada #%u)",
                 initCallCount);
        return true;  // Retorna sucesso (operação idempotente)
    }

    LOG_INFO(MODULE_NAME, "Inicializando controlador de irrigação (chamada #%u)",
             initCallCount);

    // Verifica se o hardware está disponível
    if (!Hardware::isIrrigationSafe()) {
        LOG_ERROR(MODULE_NAME, "Hardware não está seguro para irrigação");
        return false;
    }

    // Garante que a bomba inicie desligada
    Hardware::setRelayState(Hardware::RELAY_OFF);
    m_data.pumpActive = false;

    // Inicializa timestamps
    uint32_t currentTime = millis();
    m_lastRuntimeUpdate = currentTime;
    m_lastDayReset = currentTime;
    m_data.lastDecisionTime = currentTime;

    // Reset de emergência se necessário
    m_data.emergencyShutdown = false;

    m_initialized = true;

    LOG_INFO(MODULE_NAME, "Controlador inicializado com sucesso");
    LOG_INFO(MODULE_NAME, "Limiar de umidade: %.1f%% - %.1f%%",
             MOISTURE_THRESHOLD_LOW, MOISTURE_THRESHOLD_HIGH);

    return true;
}

bool IrrigationController::update() {
    if (!m_initialized) {
        return false;
    }

    bool stateChanged = false;
    uint32_t currentTime = millis();

    // Atualiza estatísticas de runtime
    updateRuntime();

    // Verifica timeouts e limites
    if (checkTimeouts()) {
        stateChanged = true;
    }

    // Reset diário (aproximadamente a cada 24 horas)
    if (currentTime - m_lastDayReset > 86400000UL) { // 24 horas em ms
        resetDailyCounters();
        m_lastDayReset = currentTime;
        LOG_INFO(MODULE_NAME, "Reset diário executado");
    }

    // Verifica condições de segurança
    if (m_data.pumpActive && !checkSafetyConditions()) {
        LOG_WARN(MODULE_NAME, "Condições de segurança falaram - desligando bomba");
        deactivate(false);
        stateChanged = true;
    }

    return stateChanged;
}

bool IrrigationController::updateDecision(const SensorData& sensorData) {
    if (!m_initialized || m_data.emergencyShutdown || m_data.manualMode) {
        return false;
    }

    uint32_t currentTime = millis();

    // Limite de frequência de decisões (não mais que a cada 5 segundos)
    if (currentTime - m_data.lastDecisionTime < 5000) {
        return false;
    }

    m_data.lastDecisionTime = currentTime;

    bool shouldActivate = false;
    bool shouldDeactivate = false;

    // Lógica de decisão baseada na umidade
    if (!m_data.pumpActive) {
        // Bomba desligada - verifica se deve ligar
        if (sensorData.humidityPercent < m_data.currentThreshold) {
            // Verifica tempo mínimo entre ativações
            if (currentTime - m_data.lastDeactivationTime >= IRRIGATION_MIN_INTERVAL) {
                shouldActivate = true;
                LOG_INFO(MODULE_NAME, "Decisão automática: ATIVAR - Umidade %.1f%% < %.1f%%",
                         sensorData.humidityPercent, m_data.currentThreshold);
            } else {
                uint32_t remaining = IRRIGATION_MIN_INTERVAL - (currentTime - m_data.lastDeactivationTime);
                LOG_DEBUG(MODULE_NAME, "Aguardando intervalo mínimo - restam %u ms", remaining);
            }
        }
    } else {
        // Bomba ligada - verifica se deve desligar
        if (sensorData.humidityPercent >= MOISTURE_THRESHOLD_HIGH) {
            shouldDeactivate = true;
            LOG_INFO(MODULE_NAME, "Decisão automática: DESATIVAR - Umidade %.1f%% >= %.1f%%",
                     sensorData.humidityPercent, MOISTURE_THRESHOLD_HIGH);
        }
    }

    // Executa a decisão
    if (shouldActivate) {
        return activateInternal(IRRIGATION_MAX_RUNTIME, false);
    } else if (shouldDeactivate) {
        return deactivate(false);
    }

    return false;
}

bool IrrigationController::activateManual(uint32_t duration) {
    if (!m_initialized) {
        LOG_ERROR(MODULE_NAME, "Controlador não inicializado");
        return false;
    }

    LOG_INFO(MODULE_NAME, "Ativação manual solicitada - duração: %u ms", duration);
    return activateInternal(duration, true);
}

bool IrrigationController::activateInternal(uint32_t duration, bool manual) {
    // Verifica condições de segurança
    if (!checkSafetyConditions()) {
        LOG_ERROR(MODULE_NAME, "Falha nas condições de segurança");
        return false;
    }

    // Verifica se já está ativa
    if (m_data.pumpActive) {
        LOG_WARN(MODULE_NAME, "Bomba já está ativa");
        return false;
    }

    uint32_t currentTime = millis();

    // Verifica intervalo mínimo apenas para ativação automática
    if (!manual && (currentTime - m_data.lastDeactivationTime < IRRIGATION_MIN_INTERVAL)) {
        LOG_WARN(MODULE_NAME, "Bloqueado: intervalo mínimo não respeitado");
        return false;
    }

    // Limita duração máxima
    if (duration > IRRIGATION_MAX_RUNTIME) {
        duration = IRRIGATION_MAX_RUNTIME;
        LOG_WARN(MODULE_NAME, "Duração limitada a %u ms por segurança", duration);
    }

    // Delay de segurança antes da ativação
    delay(IRRIGATION_ACTIVATION_DELAY);

    // Ativa o relé
    Hardware::setRelayState(Hardware::RELAY_ON);

    // Atualiza estado interno
    m_data.pumpActive = true;
    m_data.activationTime = currentTime;
    m_data.manualMode = manual;
    m_data.dailyActivations++;

    // Programa parada se duração foi especificada
    if (duration > 0) {
        m_scheduledStopTime = currentTime + duration;
    } else {
        m_scheduledStopTime = 0; // Funcionamento indefinido
    }

    LOG_INFO(MODULE_NAME, "Bomba ativada com sucesso (%s) - Ativação #%d do dia",
             manual ? "MANUAL" : "AUTOMÁTICA", m_data.dailyActivations);

    if (duration > 0) {
        LOG_INFO(MODULE_NAME, "Parada programada em %u ms", duration);
    }

    return true;
}

bool IrrigationController::deactivate(bool manual) {
    if (!m_data.pumpActive) {
        return false; // Já está desativada
    }

    uint32_t currentTime = millis();

    // Desliga o relé
    Hardware::setRelayState(Hardware::RELAY_OFF);

    // Calcula tempo de funcionamento desta sessão
    uint32_t sessionRuntime = (currentTime - m_data.activationTime) / 1000; // Em segundos

    // Atualiza estado interno
    m_data.pumpActive = false;
    m_data.lastDeactivationTime = currentTime;
    m_data.manualMode = false;
    m_scheduledStopTime = 0;

    LOG_INFO(MODULE_NAME, "Bomba desativada (%s) - Sessão: %u segundos",
             manual ? "MANUAL" : "AUTOMÁTICA", sessionRuntime);

    LOG_INFO(MODULE_NAME, "Tempo total acumulado: %u segundos", m_data.totalRuntime);

    return true;
}

bool IrrigationController::processCommand(bool activate, uint32_t duration) {
    LOG_INFO(MODULE_NAME, "Comando WebSocket: %s (duração: %u ms)",
             activate ? "ATIVAR" : "DESATIVAR", duration);

    if (activate) {
        return activateManual(duration);
    } else {
        return deactivate(true);
    }
}

void IrrigationController::emergencyShutdown() {
    LOG_FATAL(MODULE_NAME, "SHUTDOWN DE EMERGÊNCIA ATIVADO");

    // Para imediatamente
    Hardware::setRelayState(Hardware::RELAY_OFF);

    // Atualiza estado
    m_data.pumpActive = false;
    m_data.emergencyShutdown = true;
    m_data.manualMode = false;
    m_scheduledStopTime = 0;

    LOG_FATAL(MODULE_NAME, "Sistema bloqueado - requer reset manual");
}

bool IrrigationController::resetEmergency() {
    if (!m_data.emergencyShutdown) {
        return true; // Já não está em emergência
    }

    LOG_WARN(MODULE_NAME, "Resetando estado de emergência");

    // Verifica condições de segurança antes de resetar
    if (!Hardware::isIrrigationSafe()) {
        LOG_ERROR(MODULE_NAME, "Condições de segurança não atendidas - reset negado");
        return false;
    }

    m_data.emergencyShutdown = false;
    LOG_INFO(MODULE_NAME, "Estado de emergência resetado com sucesso");

    return true;
}

bool IrrigationController::checkSafetyConditions() {
    // Verifica estado de emergência
    if (m_data.emergencyShutdown) {
        return false;
    }

    // Verifica condições de hardware
    if (!Hardware::isIrrigationSafe()) {
        return false;
    }

    // Verifica se não excedeu limite diário de ativações
    if (m_data.dailyActivations > 50) { // Limite razoável
        LOG_ERROR(MODULE_NAME, "Limite diário de ativações excedido: %d", m_data.dailyActivations);
        return false;
    }

    return true;
}

bool IrrigationController::checkTimeouts() {
    if (!m_data.pumpActive) {
        return false;
    }

    uint32_t currentTime = millis();
    bool shouldStop = false;

    // Verifica parada programada
    if (m_scheduledStopTime > 0 && currentTime >= m_scheduledStopTime) {
        LOG_INFO(MODULE_NAME, "Tempo programado atingido - parando bomba");
        shouldStop = true;
    }

    // Verifica tempo máximo absoluto (segurança)
    uint32_t runningTime = currentTime - m_data.activationTime;
    if (runningTime >= IRRIGATION_MAX_RUNTIME) {
        LOG_WARN(MODULE_NAME, "Tempo máximo de segurança atingido - parando bomba");
        shouldStop = true;
    }

    if (shouldStop) {
        deactivate(false);
        return true;
    }

    return false;
}

void IrrigationController::updateRuntime() {
    if (!m_data.pumpActive) {
        return;
    }

    uint32_t currentTime = millis();
    uint32_t elapsed = (currentTime - m_lastRuntimeUpdate) / 1000; // Em segundos

    if (elapsed > 0) {
        m_data.totalRuntime += elapsed;
        m_lastRuntimeUpdate = currentTime;
    }
}

void IrrigationController::resetDailyCounters() {
    m_data.dailyActivations = 0;
    LOG_INFO(MODULE_NAME, "Contadores diários resetados");
}

bool IrrigationController::isActive() const {
    return m_data.pumpActive;
}

const IrrigationData& IrrigationController::getData() const {
    return m_data;
}

uint32_t IrrigationController::getTotalRuntime() const {
    return m_data.totalRuntime;
}

uint32_t IrrigationController::getLastActivation() const {
    return m_data.activationTime;
}

bool IrrigationController::isInitialized() const {
    return m_initialized;
}