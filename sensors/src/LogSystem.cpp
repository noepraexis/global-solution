/**
 * @file LogSystem.cpp
 * @brief Implementação do sistema de logging otimizado.
 */

#include "LogSystem.h"
#include "StringUtils.h"
#include <string.h>
#include <stdarg.h>
#include <esp_log.h>  // Para controle de logs do ESP-IDF

// ====================================================================
// Implementação do CircularLogBuffer
// ====================================================================

CircularLogBuffer* CircularLogBuffer::s_instance = nullptr;

CircularLogBuffer& CircularLogBuffer::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new CircularLogBuffer();
    }
    return *s_instance;
}

CircularLogBuffer::CircularLogBuffer()
    : m_head(0) {
    // Cria mutex para proteção de acesso
    m_mutex = xSemaphoreCreateMutex();

    // Inicializa o buffer
    memset(m_entries, 0, sizeof(m_entries));
}

CircularLogBuffer::~CircularLogBuffer() {
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

void CircularLogBuffer::addEntry(const LogEntry& entry) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Copia a entrada para a posição atual
        m_entries[m_head] = entry;

        // Avança o ponteiro de forma circular
        m_head = (m_head + 1) % LOG_BUFFER_SIZE;

        xSemaphoreGive(m_mutex);
    }
}

size_t CircularLogBuffer::getEntries(char* buffer, size_t maxSize) {
    if (!buffer || maxSize == 0) {
        return 0;
    }

    size_t totalWritten = 0;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Determina o tamanho necessário para formatar cada entrada
        const size_t entrySizeEstimate = 128;

        // Estima o número de entradas que cabem no buffer
        size_t maxEntries = maxSize / entrySizeEstimate;
        if (maxEntries > LOG_BUFFER_SIZE) {
            maxEntries = LOG_BUFFER_SIZE;
        }

        // Escreve o cabeçalho
        int written = snprintf(buffer, maxSize,
            "=== Log de Sistema (últimas %u mensagens) ===\n\n",
            (uint32_t)maxEntries);

        if (written > 0) {
            totalWritten += written;
        }

        // Índice inicial para leitura (mais recente primeiro)
        size_t index = (m_head == 0) ? LOG_BUFFER_SIZE - 1 : (m_head - 1);

        // Lê as entradas em ordem reversa (mais recente primeiro)
        for (size_t i = 0; i < maxEntries; i++) {
            const LogEntry& entry = m_entries[index];

            // Verifica se a entrada é válida (timestamp > 0)
            if (entry.timestamp > 0) {
                // Formata a timestamp como segundos.milissegundos
                uint32_t seconds = entry.timestamp / 1000;
                uint32_t milliseconds = entry.timestamp % 1000;

                // Formata a mensagem
                written = snprintf(buffer + totalWritten, maxSize - totalWritten,
                    "[%5u.%03u][%-5s][%-10s] %s\n",
                    seconds, milliseconds,
                    LogRouter::getInstance().levelToString(entry.level),
                    entry.module,
                    entry.message);

                if (written > 0 && totalWritten + written < maxSize) {
                    totalWritten += written;
                } else {
                    // Buffer cheio, adiciona indicador e sai
                    const char* truncated = "... (truncado)\n";
                    size_t truncatedLen = strlen(truncated);

                    if (totalWritten + truncatedLen < maxSize) {
                        strcpy(buffer + totalWritten, truncated);
                        totalWritten += truncatedLen;
                    }
                    break;
                }
            }

            // Move para a entrada anterior de forma circular
            index = (index == 0) ? LOG_BUFFER_SIZE - 1 : (index - 1);
        }

        xSemaphoreGive(m_mutex);
    }

    // Garante terminação null
    if (totalWritten < maxSize) {
        buffer[totalWritten] = '\0';
    } else if (maxSize > 0) {
        buffer[maxSize - 1] = '\0';
    }

    return totalWritten;
}

// ====================================================================
// Implementação do TelemetryManager
// ====================================================================

TelemetryManager* TelemetryManager::s_instance = nullptr;

TelemetryManager& TelemetryManager::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new TelemetryManager();
    }
    return *s_instance;
}

TelemetryManager::TelemetryManager()
    : m_nextToken(1) {
    // Cria mutex para proteção de acesso
    m_mutex = xSemaphoreCreateMutex();

    // Inicializa sessões
    for (int i = 0; i < MAX_TELEMETRY_SESSIONS; i++) {
        m_sessions[i].token = 0;
        m_sessions[i].active = false;
    }
}

TelemetryManager::~TelemetryManager() {
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

uint32_t TelemetryManager::beginSession(const char* name) {
    if (!name) {
        return 0;
    }

    uint32_t newToken = 0;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Procura por um slot livre
        for (int i = 0; i < MAX_TELEMETRY_SESSIONS; i++) {
            if (!m_sessions[i].active) {
                // Inicializa a nova sessão
                m_sessions[i].token = m_nextToken++;
                m_sessions[i].active = true;
                m_sessions[i].lastUpdateTime = millis();

                // Copia o nome de forma segura e otimizada
                StringUtils::safeCopyString(m_sessions[i].name, name, sizeof(m_sessions[i].name));

                newToken = m_sessions[i].token;
                break;
            }
        }

        xSemaphoreGive(m_mutex);
    }

    return newToken;
}

bool TelemetryManager::updateSession(uint32_t token, const char* fmt, ...) {
    if (token == 0 || !fmt) {
        return false;
    }

    bool result = false;

    // Verifica se o token é válido e se passou tempo suficiente desde a última atualização
    TelemetrySession* session = findSession(token);
    if (session) {
        uint32_t currentTime = millis();
        uint32_t elapsed = currentTime - session->lastUpdateTime;

        // Verifica intervalo mínimo entre atualizações
        if (elapsed >= TELEMETRY_UPDATE_INTERVAL) {
            // Formata a mensagem
            char buffer[LOG_MAX_MESSAGE_SIZE];

            va_list args;
            va_start(args, fmt);
            vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
            va_end(args);
            buffer[sizeof(buffer) - 1] = '\0';

            // Armazena o token de reserva de forma estática por sessão
            // Cada sessão precisa de seu próprio token para evitar sobreposição
            static uint32_t reserveTokens[MAX_TELEMETRY_SESSIONS] = {0};

            // Obtém índice da sessão
            int sessionIndex = -1;
            for (int i = 0; i < MAX_TELEMETRY_SESSIONS; i++) {
                if (m_sessions[i].active && m_sessions[i].token == token) {
                    sessionIndex = i;
                    break;
                }
            }

            if (sessionIndex >= 0) {
                // Obtém token de reserva na primeira vez
                if (reserveTokens[sessionIndex] == 0) {
                    reserveTokens[sessionIndex] = CONSOLE_RESERVE_LINE();
                }

                // Atualiza a linha com o token de reserva
                if (reserveTokens[sessionIndex] != 0) {
                    if (CONSOLE_UPDATE_RESERVED_LINE(reserveTokens[sessionIndex], "[%s] %s", session->name, buffer)) {
                        result = true;
                    }
                }
            }

            // Atualiza timestamp mesmo se falhar (para não tentar imediatamente de novo)
            session->lastUpdateTime = currentTime;
        }
    }

    return result;
}

bool TelemetryManager::endSession(uint32_t token) {
    if (token == 0) {
        return false;
    }

    bool result = false;
    int sessionIndex = -1;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Procura pela sessão com o token especificado
        for (int i = 0; i < MAX_TELEMETRY_SESSIONS; i++) {
            if (m_sessions[i].active && m_sessions[i].token == token) {
                // Salva o índice para referência
                sessionIndex = i;

                // Marca como inativa
                m_sessions[i].active = false;
                m_sessions[i].token = 0;

                result = true;
                break;
            }
        }

        xSemaphoreGive(m_mutex);
    }

    // Se encontrou a sessão, libera sua linha reservada
    if (result && sessionIndex >= 0) {
        // Array de tokens de reserva estático (mesma referência que em updateSession)
        static uint32_t reserveTokens[MAX_TELEMETRY_SESSIONS] = {0};

        // Se há um token de reserva para esta sessão, libera-o
        if (reserveTokens[sessionIndex] != 0) {
            CONSOLE_RELEASE_LINE(reserveTokens[sessionIndex]);
            // Limpa o token para futuras sessões
            reserveTokens[sessionIndex] = 0;
        }
    }

    return result;
}

TelemetrySession* TelemetryManager::findSession(uint32_t token) {
    TelemetrySession* result = nullptr;

    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Procura pela sessão com o token especificado
        for (int i = 0; i < MAX_TELEMETRY_SESSIONS; i++) {
            if (m_sessions[i].active && m_sessions[i].token == token) {
                result = &m_sessions[i];
                break;
            }
        }

        xSemaphoreGive(m_mutex);
    }

    return result;
}

// ====================================================================
// Implementação do LogRouter
// ====================================================================

LogRouter* LogRouter::s_instance = nullptr;

LogRouter& LogRouter::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new LogRouter();
    }
    return *s_instance;
}

LogRouter::LogRouter() {
    // Configura padrões de bloqueio do watchdog
    ConsoleFilter::addBlockedPattern("Watchdog resetado");
    ConsoleFilter::addBlockedPattern("Task watchdog got triggered");
    ConsoleFilter::addBlockedPattern("WATCHDOG-TIMER");
    ConsoleFilter::addBlockedPattern("WDT");

    // Desativa logs nativos do ESP-IDF
    esp_log_level_set("*", ESP_LOG_NONE);

    // Configurações específicas para componentes essenciais
    // que podem gerar mensagens importantes mesmo em produção
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("esp_https_ota", ESP_LOG_ERROR);
}

LogRouter::~LogRouter() {
    // Nada a fazer, os componentes são limpos em seus próprios destrutores
}

void LogRouter::log(LogLevel level, const char* module, const char* fmt, ...) {
    // Verifica se o nível de log deve ser processado
    bool shouldOutputToSerial = static_cast<int>(level) >= static_cast<int>(LOG_LEVEL_SERIAL);
    bool shouldStoreInMemory = static_cast<int>(level) >= static_cast<int>(LOG_LEVEL_MEMORY);

    // Se nenhum destino estiver configurado, retorna imediatamente
    if (!shouldOutputToSerial && !shouldStoreInMemory) {
        return;
    }

    // Prepara o nome do módulo ou usa padrão
    char moduleName[LOG_MODULE_NAME_MAX_SIZE];
    if (module && strlen(module) > 0) {
        StringUtils::safeCopyString(moduleName, module, sizeof(moduleName));
    } else {
        strcpy(moduleName, "SYS");
    }

    // Formata a mensagem
    char buffer[LOG_MAX_MESSAGE_SIZE];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);

    // Garante terminação da string
    buffer[LOG_MAX_MESSAGE_SIZE - 1] = '\0';

    // Saída para console se configurado
    if (shouldOutputToSerial) {
        // Mapeia para prioridade do ConsoleManager
        MessagePriority priority = levelToPriority(level);

        // Formata a mensagem para console com prefixo de nível e módulo
        char consoleMessage[LOG_MAX_MESSAGE_SIZE + 32];
        snprintf(consoleMessage, sizeof(consoleMessage), "[%s][%s] %s",
                levelToString(level), moduleName, buffer);

        // Envia para o ConsoleManager
        ConsoleManager::getInstance().println(consoleMessage, priority);
    }

    // Armazena em buffer circular se configurado
    if (shouldStoreInMemory) {
        // Cria entrada de log
        LogEntry entry;
        entry.timestamp = millis();
        entry.level = level;

        // Copia nome do módulo de forma segura e otimizada
        StringUtils::safeCopyString(entry.module, moduleName, sizeof(entry.module));

        // Copia mensagem de forma segura e otimizada
        StringUtils::safeCopyString(entry.message, buffer, sizeof(entry.message));

        // Adiciona ao buffer circular
        CircularLogBuffer::getInstance().addEntry(entry);
    }
}

size_t LogRouter::getStoredLogs(char* buffer, size_t maxSize) {
    return CircularLogBuffer::getInstance().getEntries(buffer, maxSize);
}

uint32_t LogRouter::beginTelemetry(const char* name) {
    return TelemetryManager::getInstance().beginSession(name);
}

bool LogRouter::updateTelemetry(uint32_t token, const char* fmt, ...) {
    if (token == 0 || !fmt) {
        return false;
    }

    // Formata a mensagem
    char buffer[LOG_MAX_MESSAGE_SIZE];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    // Encaminha para o gerenciador de telemetria
    return TelemetryManager::getInstance().updateSession(token, buffer);
}

bool LogRouter::endTelemetry(uint32_t token) {
    return TelemetryManager::getInstance().endSession(token);
}

const char* LogRouter::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARN:    return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKN";
    }
}

MessagePriority LogRouter::levelToPriority(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return MessagePriority::MSG_LOW;
        case LogLevel::DEBUG:   return MessagePriority::MSG_LOW;
        case LogLevel::INFO:    return MessagePriority::MSG_NORMAL;
        case LogLevel::WARN:    return MessagePriority::MSG_NORMAL;
        case LogLevel::ERROR:   return MessagePriority::MSG_HIGH;
        case LogLevel::FATAL:   return MessagePriority::MSG_CRITICAL;
        default:                return MessagePriority::MSG_NORMAL;
    }
}