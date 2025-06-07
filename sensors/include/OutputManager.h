/**
 * @file OutputManager.h
 * @brief Gerenciador centralizado de saída para console e WebSocket.
 */

#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <Arduino.h>
#include "LogSystem.h"
#include "ConsoleFormat.h"
#include "TelemetryBuffer.h"

// Forward declarations
class AsyncSoilWebServer;

/**
 * @class OutputManager
 * @brief Gerencia roteamento de saídas para diferentes destinos.
 *
 * Este componente atua como Facade para o sistema de saída,
 * centralizando o roteamento de mensagens para console, WebSocket
 * e buffer de memória, aplicando limites de taxa e filtragem
 * apropriados para cada tipo de mensagem.
 */
class OutputManager {
public:
    /**
     * @enum OutputDestination
     * @brief Define destinos possíveis para as mensagens.
     */
    enum class OutputDestination {
        CONSOLE_ONLY,     ///< Apenas console (desenvolvimento/diagnóstico)
        WEBSOCKET_ONLY,   ///< Apenas WebSocket (interface web)
        BOTH,             ///< Ambos (raro, apenas para alertas críticos)
        MEMORY_ONLY       ///< Apenas buffer de memória para diagnóstico
    };

    /**
     * @enum MessageType
     * @brief Define tipos de mensagens para controle de taxa.
     */
    enum class MessageType {
        DEBUG,      ///< Informações de debug (baixa frequência)
        STATUS,     ///< Informações de status (média frequência)
        TELEMETRY,  ///< Dados de telemetria (alta frequência)
        ALERT       ///< Alertas críticos (sem limite de taxa)
    };

    /**
     * @brief Inicializa o gerenciador de saída.
     */
    static void initialize();

    /**
     * @brief API principal para logging.
     *
     * @param module Nome do módulo originador da mensagem
     * @param level Nível de log (INFO, WARN, etc.)
     * @param dest Destino da mensagem (console, websocket, ambos)
     * @param fmt Formato da mensagem (estilo printf)
     * @param ... Argumentos variáveis para formato
     */
    static void log(const char* module, LogLevel level,
                   OutputDestination dest, const char* fmt, ...);

    /**
     * @brief API para telemetria.
     *
     * @param sensor Nome do sensor ou componente
     * @param data Buffer com dados de telemetria
     */
    static void telemetry(const char* sensor, TelemetryBuffer& data);

    /**
     * @brief Verifica se deve atualizar com base em tipo e destino.
     *
     * @param type Tipo de mensagem
     * @param dest Destino da mensagem
     * @return true se a atualização é permitida, false caso contrário
     */
    static bool shouldUpdate(MessageType type, OutputDestination dest);

    /**
     * @brief Configura o servidor WebSocket.
     *
     * @param server Ponteiro para o servidor WebSocket
     */
    static void attachWebSocketServer(AsyncSoilWebServer* server);

    /**
     * @brief Configura o gerenciador de console.
     *
     * @param console Ponteiro para o gerenciador de console
     */
    static void attachConsoleManager(ConsoleManager* console);

private:
    static AsyncSoilWebServer* s_webSocketServer;  ///< Servidor WebSocket
    static ConsoleManager* s_consoleManager;       ///< Gerenciador de console
    static uint32_t s_lastUpdateTime[4][3];        ///< Último tempo de atualização [tipo][destino]
    static bool s_initialized;                    ///< Flag de inicialização

    /**
     * @brief Roteia mensagem para o console.
     *
     * @param module Nome do módulo
     * @param level Nível de log
     * @param message Mensagem formatada
     */
    static void routeToConsole(const char* module, LogLevel level, const char* message);

    /**
     * @brief Roteia telemetria para o WebSocket.
     *
     * @param sensor Nome do sensor ou componente
     * @param data Buffer de telemetria
     */
    static void routeToWebSocket(const char* sensor, TelemetryBuffer& data);

    /**
     * @brief Roteia mensagem para armazenamento em memória.
     *
     * @param module Nome do módulo
     * @param level Nível de log
     * @param message Mensagem formatada
     */
    static void routeToMemory(const char* module, LogLevel level, const char* message);
};

// Macros para diagnóstico (apenas console)
#define DBG_INFO(module, ...) OutputManager::log(module, LogLevel::INFO, OutputManager::OutputDestination::CONSOLE_ONLY, __VA_ARGS__)
#define DBG_WARN(module, ...) OutputManager::log(module, LogLevel::WARN, OutputManager::OutputDestination::CONSOLE_ONLY, __VA_ARGS__)
#define DBG_ERROR(module, ...) OutputManager::log(module, LogLevel::ERROR, OutputManager::OutputDestination::CONSOLE_ONLY, __VA_ARGS__)
#define DBG_DEBUG(module, ...) OutputManager::log(module, LogLevel::DEBUG, OutputManager::OutputDestination::CONSOLE_ONLY, __VA_ARGS__)

// Macros para telemetria (apenas WebSocket)
#define TELEMETRY(sensor, data) OutputManager::telemetry(sensor, data)

// Macros para alertas (ambos os canais)
#define ALERT(module, ...) OutputManager::log(module, LogLevel::ERROR, OutputManager::OutputDestination::BOTH, __VA_ARGS__)

#endif // OUTPUT_MANAGER_H