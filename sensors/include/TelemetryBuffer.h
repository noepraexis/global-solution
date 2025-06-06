/**
 * @file TelemetryBuffer.h
 * @brief Buffer estruturado para dados de telemetria com suporte a serialização.
 */

#ifndef TELEMETRY_BUFFER_H
#define TELEMETRY_BUFFER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "StringUtils.h"

/**
 * @struct TelemetryBuffer
 * @brief Estrutura unificada para armazenamento e transmissão de dados de telemetria.
 *
 * Armazena dados de sensores e do sistema em formato estruturado
 * para facilitar a serialização tanto para o console quanto para
 * comunicação WebSocket, eliminando duplicações.
 */
struct TelemetryBuffer {
    // Dados dos sensores
    float temperature;       ///< Temperatura em graus Celsius
    float humidity;          ///< Umidade relativa do ar em percentual (0-100%)

    // Estatísticas do sistema
    uint32_t freeHeap;             ///< Heap livre em bytes
    uint16_t heapFragmentation;    ///< Fragmentação do heap em percentual
    uint32_t uptime;               ///< Tempo de atividade em segundos
    uint32_t wifiRssi;             ///< Força do sinal WiFi em dBm

    // Metadados
    uint32_t timestamp;       ///< Timestamp em milissegundos desde o boot
    uint32_t readCount;       ///< Contador de leituras
    char ipAddress[16];       ///< Endereço IP em formato string

    /**
     * @enum TelemetryType
     * @brief Define os tipos específicos de telemetria para formatação.
     */
    enum class TelemetryType {
        SENSORS,    ///< Somente dados dos sensores
        SYSTEM,     ///< Somente dados do sistema
        WIFI,       ///< Somente dados do WiFi
        ALL         ///< Todos os dados combinados
    };

    /**
     * @brief Construtor com inicializadores padrão.
     */
    TelemetryBuffer();

    /**
     * @brief Serializa o buffer para formato JSON.
     *
     * @param json Objeto JSON para serialização
     */
    void toJson(JsonObject& json) const;

    /**
     * @brief Converte o buffer para string formatada para console.
     *
     * @param buffer Buffer para armazenar a string formatada
     * @param bufferSize Tamanho do buffer
     * @param type Tipo de telemetria a ser formatada
     * @return Ponteiro para o buffer preenchido
     */
    char* toConsoleString(char* buffer, size_t bufferSize, TelemetryType type) const;
};

#endif // TELEMETRY_BUFFER_H