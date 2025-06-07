/**
 * @file ConsoleFormat.cpp
 * @brief Implementação do sistema avançado de formatação de console.
 */

#include "ConsoleFormat.h"
#include "StringUtils.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <string.h>
#include <algorithm>

// Inicialização de membros estáticos
ConsoleManager* ConsoleManager::s_instance = nullptr;
std::vector<String> ConsoleFilter::s_blockedPatterns;

// Implementação da classe ConsoleFilter
void ConsoleFilter::addBlockedPattern(const char* pattern) {
    if (pattern && strlen(pattern) > 0) {
        // Verifica se o padrão já existe na lista
        for (const auto& existingPattern : s_blockedPatterns) {
            if (existingPattern == pattern) {
                return; // Padrão já existe, não adiciona duplicado
            }
        }

        // Adiciona o novo padrão
        s_blockedPatterns.push_back(String(pattern));
    }
}

bool ConsoleFilter::shouldFilter(const char* message) {
    if (!message || strlen(message) == 0) {
        return false; // Mensagem vazia, não filtra
    }

    // Converte para String para facilitar a comparação
    String msgStr(message);

    // Verifica cada padrão bloqueado
    for (const auto& pattern : s_blockedPatterns) {
        if (msgStr.indexOf(pattern) >= 0) {
            return true; // Mensagem contém padrão bloqueado
        }
    }

    return false; // Nenhum padrão encontrado, não filtra
}

// Implementação do ConsoleManager
ConsoleManager::ConsoleManager()
    : m_lineState(LineState::NEW_LINE),
      m_lastOutputTime(0),
      m_activeReservation(0),
      m_inReservedMode(false),
      m_reservationCounter(0),
      m_historyIndex(0) {

    // Cria os semáforos para controle de acesso
    m_stateMutex = xSemaphoreCreateMutex();
    m_outputMutex = xSemaphoreCreateMutex();

    // Inicializa os buffers
    memset(m_lineBuffer, 0, sizeof(m_lineBuffer));
    memset(m_statusLineBuffer, 0, sizeof(m_statusLineBuffer));

    // Inicializa o histórico de mensagens
    memset(m_messageHistory, 0, sizeof(m_messageHistory));

    // Define padrões padrão para bloquear
    if (ConsoleFilter::s_blockedPatterns.empty()) {
        ConsoleFilter::addBlockedPattern("Watchdog resetado");
        ConsoleFilter::addBlockedPattern("Task watchdog got triggered");
        ConsoleFilter::addBlockedPattern("WATCHDOG-TIMER");
        ConsoleFilter::addBlockedPattern("WDT");
    }

    // Desativa logs do ESP-IDF para evitar interferência
    esp_log_level_set("*", ESP_LOG_NONE);
}

ConsoleManager::~ConsoleManager() {
    // Libera os recursos
    if (m_stateMutex) {
        vSemaphoreDelete(m_stateMutex);
        m_stateMutex = nullptr;
    }

    if (m_outputMutex) {
        vSemaphoreDelete(m_outputMutex);
        m_outputMutex = nullptr;
    }
}

ConsoleManager& ConsoleManager::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new ConsoleManager();
    }
    return *s_instance;
}

bool ConsoleManager::safeTakeMutex(SemaphoreHandle_t mutex, uint32_t timeoutMs) {
    if (!mutex) {
        return false;
    }

    // Tenta adquirir o mutex com timeout
    return xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void ConsoleManager::safeGiveMutex(SemaphoreHandle_t mutex) {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}

void ConsoleManager::addToHistory(const char* message, MessagePriority priority, bool isStatusLine) {
    // Atualiza o histórico em ordem circular
    LogMessage& entry = m_messageHistory[m_historyIndex];

    entry.timestamp = millis();
    entry.priority = priority;
    entry.isStatusLine = isStatusLine;

    // Usa a função utilitária para cópia segura e otimizada
    StringUtils::safeCopyString(entry.message, message, sizeof(entry.message));

    // Avança o índice do histórico
    m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;
}

bool ConsoleManager::shouldInsertBlankLine() {
    // Verifica se o último caractere escrito terminou com uma nova linha
    // e se já passou tempo suficiente desde a última saída
    uint32_t currentTime = millis();

    return m_lineState != LineState::NEW_LINE &&
           (currentTime - m_lastOutputTime > 300);
}

bool ConsoleManager::shouldAllowOutput(const char* message, MessagePriority priority) {
    // Sempre permite mensagens críticas
    if (priority == MessagePriority::MSG_CRITICAL) {
        return true;
    }

    // Verifica padrões bloqueados
    if (ConsoleFilter::shouldFilter(message)) {
        // Registra a mensagem bloqueada no histórico, mas não a exibe
        if (safeTakeMutex(m_stateMutex)) {
            addToHistory(message, priority, false);
            safeGiveMutex(m_stateMutex);
        }
        return false;
    }

    // Regras baseadas na prioridade
    if (m_inReservedMode) {
        // Em modo reservado, só permite saídas HIGH ou CRITICAL
        return priority >= MessagePriority::MSG_HIGH;
    }

    return true;
}

void ConsoleManager::logSuppressed(const char* message, MessagePriority priority) {
    if (safeTakeMutex(m_stateMutex)) {
        addToHistory(message, priority, false);
        safeGiveMutex(m_stateMutex);
    }
}

bool ConsoleManager::detectUnauthorizedOutput() {
    // Implementação básica para detectar interferências no estado do console
    if (safeTakeMutex(m_stateMutex, 200)) {
        // Verifica se o estado atual corresponde ao último estado registrado
        bool inconsistency = (m_lineState == LineState::RESERVED_LINE && !m_inReservedMode) ||
                             (m_lineState == LineState::NEW_LINE && m_inReservedMode);

        safeGiveMutex(m_stateMutex);
        return inconsistency;
    }

    // Se não conseguiu o mutex, assume que pode haver problema
    return true;
}

void ConsoleManager::recoverState() {
    // Tenta restaurar o estado do console após uma interferência
    if (safeTakeMutex(m_stateMutex, 200) && safeTakeMutex(m_outputMutex, 200)) {
        // Força uma nova linha para garantir estado limpo
        Serial.println();

        // Reinicia o estado
        m_lineState = LineState::NEW_LINE;

        // Se estávamos em modo reservado, restaura a linha de status
        if (m_inReservedMode && m_statusLineBuffer[0] != '\0') {
            Serial.print('\r');
            Serial.print(m_statusLineBuffer);
            m_lineState = LineState::RESERVED_LINE;
        }

        m_lastOutputTime = millis();

        safeGiveMutex(m_outputMutex);
        safeGiveMutex(m_stateMutex);
    }
}

void ConsoleManager::printFormatted(const char* fmt, MessagePriority priority, ...) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return; // Timeout - não conseguiu adquirir o mutex
    }

    // Formata a string no buffer temporário
    va_list args;
    va_start(args, priority);
    vsnprintf(m_lineBuffer, sizeof(m_lineBuffer) - 1, fmt, args);
    va_end(args);

    // Verifica se a mensagem deve ser filtrada
    if (!shouldAllowOutput(m_lineBuffer, priority)) {
        safeGiveMutex(m_stateMutex);
        return;
    }

    // Tenta adquirir o mutex de saída
    if (!safeTakeMutex(m_outputMutex, 200)) {
        safeGiveMutex(m_stateMutex);
        return; // Não conseguiu o mutex de saída
    }

    // Registra a mensagem no histórico
    addToHistory(m_lineBuffer, priority, false);

    // Avalia se precisamos inserir uma quebra de linha para organização
    if (shouldInsertBlankLine()) {
        Serial.println(); // Força nova linha
        m_lineState = LineState::NEW_LINE;
    }

    // Imprime o texto formatado
    Serial.print(m_lineBuffer);

    // Atualiza estado
    m_lastOutputTime = millis();
    m_lineState = LineState::MID_LINE;

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);
}

void ConsoleManager::println(const char* fmt, MessagePriority priority, ...) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return; // Timeout - não conseguiu adquirir o mutex
    }

    // Formata a string no buffer temporário
    va_list args;
    va_start(args, priority);
    vsnprintf(m_lineBuffer, sizeof(m_lineBuffer) - 1, fmt, args);
    va_end(args);

    // Verifica se a mensagem deve ser filtrada
    if (!shouldAllowOutput(m_lineBuffer, priority)) {
        safeGiveMutex(m_stateMutex);
        return;
    }

    // Tenta adquirir o mutex de saída
    if (!safeTakeMutex(m_outputMutex, 200)) {
        safeGiveMutex(m_stateMutex);
        return; // Não conseguiu o mutex de saída
    }

    // Registra a mensagem no histórico
    addToHistory(m_lineBuffer, priority, false);

    // Se estamos no meio de uma linha, adiciona quebra primeiro
    if (m_lineState != LineState::NEW_LINE) {
        Serial.println();
    }

    // Imprime a linha completa
    Serial.println(m_lineBuffer);

    // Atualiza estado
    m_lastOutputTime = millis();
    m_lineState = LineState::NEW_LINE;

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);
}

void ConsoleManager::resetOutput() {
    // Verifica se os mutex são válidos
    if (!safeTakeMutex(m_stateMutex, 300) || !safeTakeMutex(m_outputMutex, 300)) {
        // Se adquiriu o primeiro mutex mas não o segundo, libera o primeiro
        if (m_stateMutex) {
            safeGiveMutex(m_stateMutex);
        }
        return; // Timeout - não conseguiu adquirir os mutexes
    }

    // Limpa o buffer serial
    Serial.flush();

    // Garante que não há mensagens pendentes interrompendo nosso fluxo
    delay(10);

    // Adiciona várias quebras de linha para separação visual e limpeza
    Serial.println();
    Serial.println();
    Serial.println();

    // Limpa qualquer caractere parcial ou resíduo de buffer
    for (int i = 0; i < 5; i++) {
        Serial.print(' ');
    }
    Serial.println();

    // Reinicia estado
    m_lastOutputTime = millis();
    m_lineState = LineState::NEW_LINE;
    m_inReservedMode = false;
    m_activeReservation = 0;

    // Limpa os buffers para evitar resíduos
    memset(m_lineBuffer, 0, sizeof(m_lineBuffer));
    memset(m_statusLineBuffer, 0, sizeof(m_statusLineBuffer));

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);
}

void ConsoleManager::beginSection(const char* title, MessagePriority priority) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return; // Timeout - não conseguiu adquirir o mutex
    }

    // Se estamos em modo reservado e a prioridade não é suficiente, recusa
    if (m_inReservedMode && priority < MessagePriority::MSG_HIGH) {
        safeGiveMutex(m_stateMutex);
        return;
    }

    // Tenta adquirir o mutex de saída
    if (!safeTakeMutex(m_outputMutex, 200)) {
        safeGiveMutex(m_stateMutex);
        return; // Não conseguiu o mutex de saída
    }

    // Se não estamos no início de uma linha, adiciona quebra primeiro
    if (m_lineState != LineState::NEW_LINE) {
        Serial.println();
    }

    // Adiciona linha em branco para separação
    Serial.println();

    // Imprime cabeçalho da seção
    Serial.println("----------------------------------------");
    Serial.println(title);
    Serial.println("----------------------------------------");

    // Atualiza estado
    m_lastOutputTime = millis();
    m_lineState = LineState::NEW_LINE;

    // Adiciona ao histórico
    char sectionHeader[64];
    snprintf(sectionHeader, sizeof(sectionHeader), "BEGIN SECTION: %s", title);
    addToHistory(sectionHeader, priority, false);

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);
}

void ConsoleManager::endSection(MessagePriority priority) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return; // Timeout - não conseguiu adquirir o mutex
    }

    // Se estamos em modo reservado e a prioridade não é suficiente, recusa
    if (m_inReservedMode && priority < MessagePriority::MSG_HIGH) {
        safeGiveMutex(m_stateMutex);
        return;
    }

    // Tenta adquirir o mutex de saída
    if (!safeTakeMutex(m_outputMutex, 200)) {
        safeGiveMutex(m_stateMutex);
        return; // Não conseguiu o mutex de saída
    }

    // Se não estamos no início de uma linha, adiciona quebra primeiro
    if (m_lineState != LineState::NEW_LINE) {
        Serial.println();
    }

    // Imprime rodapé da seção
    Serial.println("----------------------------------------");

    // Atualiza estado
    m_lastOutputTime = millis();
    m_lineState = LineState::NEW_LINE;

    // Adiciona ao histórico
    addToHistory("END SECTION", priority, false);

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);
}

void ConsoleManager::updateLine(const char* fmt, ...) {
    // Simples redirecionamento para a implementação segura
    va_list args;
    va_start(args, fmt);

    // Verifica se já estamos em modo reservado
    if (m_inReservedMode) {
        // Se já estamos em modo reservado, mas sem um token ativo (modo degradado)
        // atualizamos conforme solicitado
        char buffer[sizeof(m_lineBuffer)];
        vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
        va_end(args);

        // Chama a implementação segura sem token
        if (safeTakeMutex(m_stateMutex, 200) && safeTakeMutex(m_outputMutex, 200)) {
            // Atualiza o buffer de status de forma segura e otimizada
            StringUtils::safeCopyString(m_statusLineBuffer, buffer, sizeof(m_statusLineBuffer));

            // Adiciona espaços adicionais para limpar caracteres restantes
            size_t len = strlen(m_statusLineBuffer);
            if (len < sizeof(m_statusLineBuffer) - 15) {
                memset(m_statusLineBuffer + len, ' ', 15);
                m_statusLineBuffer[len + 15] = '\0';
            }

            // Retorno de carro para início da linha e imprime
            Serial.print('\r');
            Serial.print(m_statusLineBuffer);

            // Adiciona ao histórico
            addToHistory(buffer, MessagePriority::MSG_HIGH, true);

            // Atualiza estado
            m_lastOutputTime = millis();
            m_lineState = LineState::RESERVED_LINE;

            safeGiveMutex(m_outputMutex);
            safeGiveMutex(m_stateMutex);
        }
        return;
    }

    // Não estamos em modo reservado, então tentamos reservar
    uint32_t token = reserveLine();

    // Formata a string no buffer
    char buffer[sizeof(m_lineBuffer)];
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);

    // Atualiza a linha com o token obtido
    updateLineWithToken(buffer, token);
}

uint32_t ConsoleManager::reserveLine() {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return 0; // Timeout - não conseguiu adquirir o mutex
    }

    // Se já estamos em modo reservado, retorna o token ativo (uso compartilhado)
    if (m_inReservedMode) {
        uint32_t token = m_activeReservation;
        safeGiveMutex(m_stateMutex);
        return token;
    }

    // Gera um novo token único
    uint32_t token = (millis() & 0xFFFF0000) | (++m_reservationCounter & 0xFFFF);

    // Atualiza o estado
    m_activeReservation = token;
    m_inReservedMode = true;

    // Se necessário, adiciona quebra de linha para começar linha limpa
    if (safeTakeMutex(m_outputMutex, 200)) {
        if (m_lineState != LineState::NEW_LINE) {
            Serial.println();
        }

        safeGiveMutex(m_outputMutex);
    }

    // Adiciona ao histórico
    char reserveMsg[64];
    snprintf(reserveMsg, sizeof(reserveMsg), "RESERVED LINE (token: %u)", token);
    addToHistory(reserveMsg, MessagePriority::MSG_NORMAL, true);

    safeGiveMutex(m_stateMutex);
    return token;
}

bool ConsoleManager::releaseLine(uint32_t token) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return false; // Timeout - não conseguiu adquirir o mutex
    }

    // Verifica se o token é válido
    if (!m_inReservedMode || m_activeReservation != token) {
        safeGiveMutex(m_stateMutex);
        return false;
    }

    // Libera a reserva
    m_inReservedMode = false;
    m_activeReservation = 0;

    // Adiciona quebra de linha após linha reservada
    if (safeTakeMutex(m_outputMutex, 200)) {
        Serial.println();
        m_lineState = LineState::NEW_LINE;
        safeGiveMutex(m_outputMutex);
    }

    // Adiciona ao histórico
    char releaseMsg[64];
    snprintf(releaseMsg, sizeof(releaseMsg), "RELEASED LINE (token: %u)", token);
    addToHistory(releaseMsg, MessagePriority::MSG_NORMAL, true);

    safeGiveMutex(m_stateMutex);
    return true;
}

bool ConsoleManager::updateLineWithToken(const char* fmt, uint32_t token, ...) {
    // Verifica se o mutex é válido
    if (!safeTakeMutex(m_stateMutex, 200)) {
        return false; // Timeout - não conseguiu adquirir o mutex
    }

    // Verifica se o token é válido para atualização
    bool validToken = m_inReservedMode && (m_activeReservation == token || token == 0);
    if (!validToken) {
        safeGiveMutex(m_stateMutex);
        return false;
    }

    // Formata a string no buffer temporário
    va_list args;
    va_start(args, token);
    vsnprintf(m_lineBuffer, sizeof(m_lineBuffer) - 1, fmt, args);
    va_end(args);

    // Tenta adquirir o mutex de saída
    if (!safeTakeMutex(m_outputMutex, 200)) {
        safeGiveMutex(m_stateMutex);
        return false; // Não conseguiu o mutex de saída
    }

    // Atualiza o buffer de status de forma segura e otimizada
    StringUtils::safeCopyString(m_statusLineBuffer, m_lineBuffer, sizeof(m_statusLineBuffer));

    // Adiciona espaços adicionais para limpar caracteres restantes
    size_t len = strlen(m_statusLineBuffer);
    if (len < sizeof(m_statusLineBuffer) - 15) {
        memset(m_statusLineBuffer + len, ' ', 15);
        m_statusLineBuffer[len + 15] = '\0';
    }

    // Verifica se outra mensagem interrompeu nossa linha
    uint32_t now = millis();
    bool interrupted = (m_lineState != LineState::RESERVED_LINE && m_lineState != LineState::NEW_LINE) ||
                       (now - m_lastOutputTime > 300);

    if (interrupted) {
        // Força quebra de linha se houve interrupção
        Serial.println();
    }

    // Retorno de carro e imprime a linha atualizada
    Serial.print('\r');
    Serial.print(m_statusLineBuffer);

    // Adiciona ao histórico
    addToHistory(m_lineBuffer, MessagePriority::MSG_HIGH, true);

    // Atualiza estado
    m_lastOutputTime = now;
    m_lineState = LineState::RESERVED_LINE;

    safeGiveMutex(m_outputMutex);
    safeGiveMutex(m_stateMutex);

    return true;
}