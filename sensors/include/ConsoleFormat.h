/**
 * @file ConsoleFormat.h
 * @brief Sistema avançado de controle de saída para console com formatação, sincronização e priorização.
 */

#ifndef CONSOLE_FORMAT_H
#define CONSOLE_FORMAT_H

#include <Arduino.h>
#include <mutex>
#include <esp_log.h>  // Para controle de logs do ESP-IDF
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @enum MessagePriority
 * @brief Define níveis de prioridade para mensagens de console.
 */
enum class MessagePriority {
    MSG_LOW,      ///< Mensagens de baixa prioridade (diagnóstico, detalhes)
    MSG_NORMAL,   ///< Mensagens de prioridade normal (informações gerais)
    MSG_HIGH,     ///< Mensagens de alta prioridade (erros, avisos importantes)
    MSG_CRITICAL  ///< Mensagens críticas (nunca devem ser suprimidas)
};

/**
 * @class ConsoleFilter
 * @brief Filtra e gerencia padrões de mensagens para controle de saída.
 */
class ConsoleFilter {
public:
    /**
     * @brief Adiciona um padrão à lista de bloqueados.
     * @param pattern Padrão de texto a ser bloqueado.
     */
    static void addBlockedPattern(const char* pattern);

    /**
     * @brief Verifica se uma mensagem deve ser filtrada.
     * @param message Mensagem a ser verificada.
     * @return true se a mensagem deve ser filtrada (bloqueada), false caso contrário.
     */
    static bool shouldFilter(const char* message);

    /**
     * @brief Lista de padrões de texto a serem bloqueados.
     * Tornado público para permitir acesso direto a partir do ConsoleManager.
     */
    static std::vector<String> s_blockedPatterns;
};

/**
 * @struct LogMessage
 * @brief Estrutura para armazenar mensagens de log no histórico.
 */
struct LogMessage {
    uint32_t timestamp;           ///< Timestamp da mensagem
    MessagePriority priority;     ///< Prioridade da mensagem
    char message[64];             ///< Versão truncada da mensagem
    bool isStatusLine;            ///< Indica se é uma linha de status (updateLine)
};

/**
 * @class ConsoleManager
 * @brief Gerencia saída de console sincronizada e formatada com controle avançado.
 *
 * Implementa um gerenciador de saída para console que garante alinhamento,
 * previne entrelaçamento de mensagens, fornece formatação consistente e
 * prioriza atualizações em tempo real.
 */
class ConsoleManager {
public:
    /**
     * @brief Obtém instância única do gerenciador de console.
     * @return Referência à instância singleton.
     */
    static ConsoleManager& getInstance();

    /**
     * @brief Escreve texto formatado para o console de forma sincronizada.
     *
     * @param fmt String de formato similar a printf.
     * @param priority Prioridade da mensagem (default: NORMAL).
     * @param ... Argumentos variáveis para formato.
     */
    void printFormatted(const char* fmt, MessagePriority priority = MessagePriority::MSG_NORMAL, ...);

    /**
     * @brief Escreve uma linha completa de forma sincronizada.
     *
     * @param fmt String de formato similar a printf.
     * @param priority Prioridade da mensagem (default: NORMAL).
     * @param ... Argumentos variáveis para formato.
     */
    void println(const char* fmt, MessagePriority priority = MessagePriority::MSG_NORMAL, ...);

    /**
     * @brief Limpa buffer serial e reinicia estado de saída.
     */
    void resetOutput();

    /**
     * @brief Inicia uma nova seção com cabeçalho formatado.
     * @param title Título da seção.
     * @param priority Prioridade da seção (default: HIGH).
     */
    void beginSection(const char* title, MessagePriority priority = MessagePriority::MSG_HIGH);

    /**
     * @brief Finaliza uma seção com divisor.
     * @param priority Prioridade da seção (default: HIGH).
     */
    void endSection(MessagePriority priority = MessagePriority::MSG_HIGH);

    /**
     * @brief Atualiza texto na linha atual (sobrescreve).
     *
     * Utiliza \r (retorno de carro) para atualizar a linha atual
     * sem avançar para a próxima linha, ideal para atualizações
     * em tempo real. Este método implementa proteções para garantir
     * que a atualização de linha não seja interferida por outras mensagens.
     *
     * @param fmt String de formato similar a printf.
     * @param ... Argumentos variáveis para formato.
     */
    void updateLine(const char* fmt, ...);

    /**
     * @brief Reserva linha para atualizações em tempo real.
     * @return Token de reserva, utilizado para validar operações na linha reservada.
     */
    uint32_t reserveLine();

    /**
     * @brief Libera a reserva de linha.
     * @param token Token de reserva obtido anteriormente.
     * @return true se a reserva foi liberada com sucesso, false caso contrário.
     */
    bool releaseLine(uint32_t token);

    /**
     * @brief Atualiza linha reservada com token de validação.
     *
     * Versão de updateLine que requer token de reserva para garantir
     * que apenas o proprietário da reserva pode atualizar a linha.
     *
     * @param fmt String de formato similar a printf.
     * @param token Token de reserva.
     * @param ... Argumentos variáveis para formato.
     * @return true se a atualização foi bem-sucedida, false caso contrário.
     */
    bool updateLineWithToken(const char* fmt, uint32_t token, ...);

    /**
     * @brief Detecta se algum output não autorizado ocorreu.
     *
     * Verifica se houve interferência externa que possa ter comprometido
     * o estado do console, como writes diretos ao Serial.
     *
     * @return true se interferência foi detectada, false caso contrário.
     */
    bool detectUnauthorizedOutput();

    /**
     * @brief Recupera o estado de console após interferência.
     *
     * Tenta restaurar o estado do console para um estado consistente
     * após uma interferência ter sido detectada.
     */
    void recoverState();

    /**
     * @brief Registra uma mensagem suprimida.
     * @param message Mensagem que foi suprimida.
     * @param priority Prioridade da mensagem.
     */
    void logSuppressed(const char* message, MessagePriority priority = MessagePriority::MSG_LOW);

    /**
     * @brief Verifica se uma saída para o console deve ser permitida.
     * @param message Mensagem a ser verificada.
     * @param priority Prioridade da mensagem.
     * @return true se a saída deve ser permitida, false caso contrário.
     */
    bool shouldAllowOutput(const char* message, MessagePriority priority = MessagePriority::MSG_NORMAL);

private:
    // Estado da linha atual
    enum class LineState {
        UNKNOWN,      ///< Estado indeterminado
        NEW_LINE,     ///< No início de uma nova linha
        MID_LINE,     ///< No meio de uma linha (depois de print, antes de println)
        RESERVED_LINE ///< Linha reservada para atualizações in-place
    };

    // Mutex para sincronização
    SemaphoreHandle_t m_stateMutex;  ///< Protege o estado interno
    SemaphoreHandle_t m_outputMutex; ///< Protege operações de saída

    // Buffers e estado
    char m_lineBuffer[256];      ///< Buffer para formatação de mensagens
    char m_statusLineBuffer[256]; ///< Buffer dedicado para linha de status
    LineState m_lineState;       ///< Estado atual da linha
    uint32_t m_lastOutputTime;   ///< Timestamp da última saída
    uint32_t m_activeReservation; ///< Token de reserva ativo (0 = nenhum)
    bool m_inReservedMode;       ///< Indica se estamos em modo de linha reservada
    uint16_t m_reservationCounter; ///< Contador para geração de tokens

    // Histórico de mensagens
    static const size_t HISTORY_SIZE = 20;
    LogMessage m_messageHistory[HISTORY_SIZE];
    size_t m_historyIndex;

    // Instância Singleton
    static ConsoleManager* s_instance;

    // Métodos privados
    ConsoleManager();
    ~ConsoleManager();

    /**
     * @brief Adiciona mensagem ao histórico.
     * @param message Mensagem para adicionar.
     * @param priority Prioridade da mensagem.
     * @param isStatusLine Flag indicando se é linha de status.
     */
    void addToHistory(const char* message, MessagePriority priority, bool isStatusLine);

    /**
     * @brief Adquire mutex com timeout e backup.
     * @param mutex Mutex a ser adquirido.
     * @param timeoutMs Timeout em milissegundos.
     * @return true se o mutex foi adquirido, false caso contrário.
     */
    bool safeTakeMutex(SemaphoreHandle_t mutex, uint32_t timeoutMs = 100);

    /**
     * @brief Libera mutex com verificação.
     * @param mutex Mutex a ser liberado.
     */
    void safeGiveMutex(SemaphoreHandle_t mutex);

    /**
     * @brief Verifica se devemos mostrar uma linha em branco antes.
     * @return true se uma linha em branco deve ser mostrada.
     */
    bool shouldInsertBlankLine();
};

// Macros para uso simplificado
#define CONSOLE_PRINT(fmt, ...) ConsoleManager::getInstance().printFormatted(fmt, MessagePriority::MSG_NORMAL, ##__VA_ARGS__)
#define CONSOLE_PRINT_HIGH(fmt, ...) ConsoleManager::getInstance().printFormatted(fmt, MessagePriority::MSG_HIGH, ##__VA_ARGS__)
#define CONSOLE_PRINTLN(fmt, ...) ConsoleManager::getInstance().println(fmt, MessagePriority::MSG_NORMAL, ##__VA_ARGS__)
#define CONSOLE_PRINTLN_HIGH(fmt, ...) ConsoleManager::getInstance().println(fmt, MessagePriority::MSG_HIGH, ##__VA_ARGS__)
#define CONSOLE_RESET() ConsoleManager::getInstance().resetOutput()
#define CONSOLE_BEGIN_SECTION(title) ConsoleManager::getInstance().beginSection(title, MessagePriority::MSG_HIGH)
#define CONSOLE_END_SECTION() ConsoleManager::getInstance().endSection(MessagePriority::MSG_HIGH)
#define CONSOLE_UPDATE_LINE(fmt, ...) ConsoleManager::getInstance().updateLine(fmt, ##__VA_ARGS__)

// Macros para linha reservada
#define CONSOLE_RESERVE_LINE() ConsoleManager::getInstance().reserveLine()
#define CONSOLE_RELEASE_LINE(token) ConsoleManager::getInstance().releaseLine(token)
#define CONSOLE_UPDATE_RESERVED_LINE(token, fmt, ...) ConsoleManager::getInstance().updateLineWithToken(fmt, token, ##__VA_ARGS__)

// Macros para filtragem
#define CONSOLE_BLOCK_PATTERN(pattern) ConsoleFilter::addBlockedPattern(pattern)

#endif // CONSOLE_FORMAT_H