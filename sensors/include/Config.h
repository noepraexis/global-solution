/**
 * @file Config.h
 * @brief Define as configurações gerais do sistema.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Versão do firmware
#define FIRMWARE_VERSION          "4.0.0"

// Configurações de WiFi
#define WIFI_SSID                 "Wokwi-GUEST"
#define WIFI_PASSWORD             ""
#define WIFI_CONNECTION_TIMEOUT   10000  // Tempo máximo para conexão WiFi (ms)
#define WIFI_MAX_RECONNECT_ATTEMPTS 20   // Máximo de tentativas de reconexão
#define WIFI_RECONNECT_INTERVAL   2000   // Intervalo entre tentativas (ms)

// Portas e interfaces
#define WEB_SERVER_PORT           80
#define SERIAL_BAUD_RATE          115200

// Configurações de sensores
#define SENSOR_CHECK_INTERVAL     200    // Intervalo de verificação dos sensores (ms)

// Configurações de irrigação
#define IRRIGATION_MAX_RUNTIME    300000  // Tempo máximo contínuo de irrigação (ms) - 5 minutos
#define IRRIGATION_MIN_INTERVAL   60000   // Intervalo mínimo entre ativações (ms) - 1 minuto
#define IRRIGATION_ACTIVATION_DELAY 500   // Delay antes de ativar relé (ms)
#define MOISTURE_THRESHOLD_LOW    30.0f   // Limiar inferior para ativação da irrigação (%)
#define MOISTURE_THRESHOLD_HIGH   70.0f   // Limiar superior para desativação da irrigação (%)

// Configurações de memória
#define USE_STATIC_MEMORY         true   // Usar alocação estática onde possível
#define JSON_BUFFER_SIZE          128    // Tamanho do buffer para JSON (bytes)
#define MAX_HTML_CLIENTS          5      // Número máximo de clientes HTML simultâneos

// Configurações de watchdog
#define WATCHDOG_TIMEOUT          5000   // Tempo limite do watchdog (ms)
#define ENABLE_TASK_WATCHDOG      true   // Habilita o Task Watchdog

// Configurações de CPU
#define TASK_SENSOR_CORE          0      // Core para tarefa de sensores
#define TASK_WEB_CORE             1      // Core para tarefa do servidor web
#define TASK_STACK_SIZE           4096   // Tamanho da pilha para tarefas (bytes)
#define TASK_PRIORITY_SENSOR      2      // Prioridade da tarefa de sensores
#define TASK_PRIORITY_WEB         1      // Prioridade da tarefa web

// Debug
#ifndef DEBUG_MODE
#define DEBUG_MODE                false  // Modo de depuração
#endif
#ifndef DEBUG_MEMORY
#define DEBUG_MEMORY              false  // Depuração de uso de memória
#endif

// ==========================================
// Configurações do Sistema de Logging
// ==========================================

/**
 * @enum LogLevel
 * @brief Níveis de log para o sistema de logging otimizado.
 */
enum class LogLevel {
    TRACE = 0,  // Informações extremamente detalhadas
    DEBUG = 1,  // Informações para depuração
    INFO = 2,   // Informações operacionais normais
    WARN = 3,   // Avisos que não impedem o funcionamento
    ERROR = 4,  // Erros que afetam funcionalidades
    FATAL = 5,  // Erros críticos que comprometem o sistema
    NONE = 6    // Desabilita todos os logs
};

// Modo de produção - define comportamento de logs
#ifndef PRODUCTION_MODE
#define PRODUCTION_MODE              false  // Modo de produção
#endif

// Nível mínimo de log para saída serial
#define LOG_LEVEL_SERIAL            (PRODUCTION_MODE ? LogLevel::ERROR : LogLevel::INFO)

// Nível mínimo para armazenamento em buffer circular
#define LOG_LEVEL_MEMORY            LogLevel::WARN

// Tamanho do buffer circular (número de mensagens)
#define LOG_BUFFER_SIZE             50

// Tamanho máximo de uma mensagem de log (bytes)
#define LOG_MAX_MESSAGE_SIZE        256

// Intervalo mínimo entre atualizações de telemetria (ms)
#define TELEMETRY_UPDATE_INTERVAL   250

// Número máximo de sessões de telemetria simultâneas
#define MAX_TELEMETRY_SESSIONS      5

// Tamanho máximo do nome de um módulo
#define LOG_MODULE_NAME_MAX_SIZE    16

/**
 * Wrapper para impressão de depuração.
 *
 * Imprime mensagens apenas se DEBUG_MODE estiver ativado.
 *
 * @param fmt Formato da mensagem (estilo printf).
 * @param ... Argumentos variáveis para o formato.
 */
#ifdef DEBUG_PRINT
#undef DEBUG_PRINT
#endif

#define APP_DEBUG_PRINT(fmt, ...) \
    do { if (DEBUG_MODE) Serial.printf(fmt, ##__VA_ARGS__); } while (0)

/**
 * Wrapper para impressão de depuração de memória.
 *
 * Imprime mensagens de depuração de memória apenas se DEBUG_MEMORY
 * estiver ativado.
 *
 * @param fmt Formato da mensagem (estilo printf).
 * @param ... Argumentos variáveis para o formato.
 */
#define DEBUG_MEMORY_PRINT(fmt, ...) \
    do { if (DEBUG_MEMORY) Serial.printf(fmt, ##__VA_ARGS__); } while (0)

#endif // CONFIG_H