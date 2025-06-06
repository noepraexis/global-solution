/**
 * @file LogSystem.h
 * @brief Sistema de logging otimizado com roteamento inteligente e controle de níveis.
 */

#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "ConsoleFormat.h"

/**
 * @struct LogEntry
 * @brief Estrutura para armazenar uma entrada de log no buffer circular.
 */
struct LogEntry {
    uint32_t timestamp;               ///< Timestamp em milissegundos
    LogLevel level;                   ///< Nível de log
    char module[LOG_MODULE_NAME_MAX_SIZE]; ///< Nome do módulo de origem
    char message[LOG_MAX_MESSAGE_SIZE];   ///< Mensagem de log
};

/**
 * @struct TelemetrySession
 * @brief Estrutura para gerenciar uma sessão de telemetria.
 */
struct TelemetrySession {
    uint32_t token;                   ///< Token único da sessão
    char name[LOG_MODULE_NAME_MAX_SIZE]; ///< Nome da sessão
    uint32_t lastUpdateTime;          ///< Último momento de atualização
    bool active;                      ///< Indica se a sessão está ativa
};

/**
 * @class CircularLogBuffer
 * @brief Implementa um buffer circular para armazenamento de logs.
 */
class CircularLogBuffer {
public:
    /**
     * @brief Obtém a instância única do buffer circular.
     * @return Referência à instância singleton.
     */
    static CircularLogBuffer& getInstance();

    /**
     * @brief Adiciona uma entrada ao buffer circular.
     * @param entry Entrada de log a ser adicionada.
     */
    void addEntry(const LogEntry& entry);

    /**
     * @brief Obtém as entradas armazenadas no buffer.
     * @param buffer Buffer para armazenar as entradas formatadas.
     * @param maxSize Tamanho máximo do buffer.
     * @return Número de bytes escritos no buffer.
     */
    size_t getEntries(char* buffer, size_t maxSize);

private:
    CircularLogBuffer();
    ~CircularLogBuffer();

    // Impede cópia e atribuição
    CircularLogBuffer(const CircularLogBuffer&) = delete;
    CircularLogBuffer& operator=(const CircularLogBuffer&) = delete;

    // Dados do buffer
    LogEntry m_entries[LOG_BUFFER_SIZE];   ///< Buffer circular de entradas
    size_t m_head;                         ///< Posição da próxima escrita
    SemaphoreHandle_t m_mutex;             ///< Mutex para acesso thread-safe

    // Instância singleton
    static CircularLogBuffer* s_instance;
};

/**
 * @class TelemetryManager
 * @brief Gerencia sessões de telemetria em tempo real.
 */
class TelemetryManager {
public:
    /**
     * @brief Obtém a instância única do gerenciador de telemetria.
     * @return Referência à instância singleton.
     */
    static TelemetryManager& getInstance();

    /**
     * @brief Inicia uma nova sessão de telemetria.
     * @param name Nome da sessão.
     * @return Token da sessão ou 0 em caso de falha.
     */
    uint32_t beginSession(const char* name);

    /**
     * @brief Atualiza dados de uma sessão de telemetria.
     * @param token Token da sessão.
     * @param fmt String de formato.
     * @param ... Argumentos variáveis.
     * @return true se a atualização foi realizada, false caso contrário.
     */
    bool updateSession(uint32_t token, const char* fmt, ...);

    /**
     * @brief Finaliza uma sessão de telemetria.
     * @param token Token da sessão.
     * @return true se a sessão foi finalizada, false caso contrário.
     */
    bool endSession(uint32_t token);

private:
    TelemetryManager();
    ~TelemetryManager();

    // Impede cópia e atribuição
    TelemetryManager(const TelemetryManager&) = delete;
    TelemetryManager& operator=(const TelemetryManager&) = delete;

    // Dados de telemetria
    TelemetrySession m_sessions[MAX_TELEMETRY_SESSIONS]; ///< Sessões ativas
    uint32_t m_nextToken;                               ///< Próximo token a ser atribuído
    SemaphoreHandle_t m_mutex;                          ///< Mutex para acesso thread-safe

    // Métodos auxiliares
    TelemetrySession* findSession(uint32_t token);

    // Instância singleton
    static TelemetryManager* s_instance;
};

/**
 * @class LogRouter
 * @brief Gerencia o roteamento de logs para diferentes destinos.
 */
class LogRouter {
public:
    /**
     * @brief Obtém a instância única do router de logs.
     * @return Referência à instância singleton.
     */
    static LogRouter& getInstance();

    /**
     * @brief Registra uma mensagem de log.
     * @param level Nível de log.
     * @param module Nome do módulo (opcional).
     * @param fmt String de formato.
     * @param ... Argumentos variáveis.
     */
    void log(LogLevel level, const char* module, const char* fmt, ...);

    /**
     * @brief Obtém logs armazenados em buffer.
     * @param buffer Buffer para armazenar logs.
     * @param maxSize Tamanho máximo do buffer.
     * @return Número de bytes escritos no buffer.
     */
    size_t getStoredLogs(char* buffer, size_t maxSize);

    /**
     * @brief Inicia uma sessão de telemetria.
     * @param name Nome da sessão.
     * @return Token da sessão ou 0 em caso de falha.
     */
    uint32_t beginTelemetry(const char* name);

    /**
     * @brief Atualiza dados de telemetria.
     * @param token Token da sessão.
     * @param fmt String de formato.
     * @param ... Argumentos variáveis.
     * @return true se a atualização foi realizada, false caso contrário.
     */
    bool updateTelemetry(uint32_t token, const char* fmt, ...);

    /**
     * @brief Finaliza uma sessão de telemetria.
     * @param token Token da sessão.
     * @return true se a sessão foi finalizada, false caso contrário.
     */
    bool endTelemetry(uint32_t token);

    /**
     * @brief Converte um nível de log para string.
     * @param level Nível de log.
     * @return String representando o nível.
     */
    const char* levelToString(LogLevel level);

    /**
     * @brief Converte um nível de log para prioridade do ConsoleManager.
     * @param level Nível de log.
     * @return Prioridade equivalente.
     */
    MessagePriority levelToPriority(LogLevel level);

private:
    LogRouter();
    ~LogRouter();

    // Impede cópia e atribuição
    LogRouter(const LogRouter&) = delete;
    LogRouter& operator=(const LogRouter&) = delete;

    // Instância singleton
    static LogRouter* s_instance;
};

// Macros para facilitar o uso
#define LOG_TRACE(module, fmt, ...) LogRouter::getInstance().log(LogLevel::TRACE, module, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(module, fmt, ...) LogRouter::getInstance().log(LogLevel::DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...)  LogRouter::getInstance().log(LogLevel::INFO, module, fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...)  LogRouter::getInstance().log(LogLevel::WARN, module, fmt, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...) LogRouter::getInstance().log(LogLevel::ERROR, module, fmt, ##__VA_ARGS__)
#define LOG_FATAL(module, fmt, ...) LogRouter::getInstance().log(LogLevel::FATAL, module, fmt, ##__VA_ARGS__)

// Macros para telemetria
#define TELEMETRY_BEGIN(name) LogRouter::getInstance().beginTelemetry(name)
#define TELEMETRY_UPDATE(token, fmt, ...) LogRouter::getInstance().updateTelemetry(token, fmt, ##__VA_ARGS__)
#define TELEMETRY_END(token) LogRouter::getInstance().endTelemetry(token)

#endif // LOG_SYSTEM_H