/**
 * @file DataTypes.h
 * @brief Define estruturas e tipos de dados otimizados para o sistema.
 */

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <Arduino.h>
#include "Config.h"

/**
 * Representa os valores brutos dos sensores.
 * Estrutura compacta otimizada para uso mínimo de memória.
 */
struct SensorRawData {
    float temperatureRaw;     // Valor bruto da temperatura do DHT22 (°C)
    float humidityRaw;        // Valor bruto da umidade do DHT22 (%)
    uint32_t timestamp;       // Timestamp da leitura (ms desde boot)

    // Construtor com valores padrão
    SensorRawData() : temperatureRaw(0.0f), humidityRaw(0.0f), timestamp(0) {}
};

/**
 * Representa os valores processados dos sensores.
 * Contém os dados convertidos para unidades físicas.
 */
struct SensorData {
    float temperature;       // Temperatura em graus Celsius
    float humidityPercent;   // Umidade relativa do ar em percentual (0-100%)
    uint32_t timestamp;      // Timestamp da leitura

    // Construtor com valores padrão
    SensorData() : temperature(0.0f), humidityPercent(0.0f), timestamp(0) {}

    /**
     * Converte dados brutos para formato físico.
     *
     * @param raw Dados brutos dos sensores.
     * @return Referência para o próprio objeto.
     */
    SensorData &fromRaw(const SensorRawData &raw) {
        // Temperatura e umidade já vêm em unidades físicas do DHT22
        // Aplica uma correção para a temperatura (valor real do DHT22)
        temperature = raw.temperatureRaw;

        // A umidade já está correta, não precisa de ajuste
        humidityPercent = raw.humidityRaw;

        // Mantém o timestamp
        timestamp = raw.timestamp;

        return *this;
    }

    /**
     * Converte os dados para string JSON.
     *
     * @param buffer Buffer para armazenar o JSON.
     * @param size Tamanho do buffer.
     * @return true se a conversão foi bem-sucedida, false caso contrário.
     */
    bool toJsonString(char *buffer, size_t size) const {
        if (!buffer || size == 0) return false;

        int written = snprintf(buffer, size,
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%u}",
            temperature, humidityPercent,
            timestamp);

        return (written > 0 && written < static_cast<int>(size));
    }
};

/**
 * Estrutura para estatísticas do sistema.
 * Monitora o desempenho do sistema.
 */
struct SystemStats {
    uint32_t freeHeap;          // Liberação de Heap em bytes
    uint32_t minFreeHeap;       // Mínimo de liberação de heap já registrado
    uint16_t heapFragmentation; // Fragmentação do heap (percentual)
    uint8_t  cpuLoad;           // Utilização da CPU (percentual)
    uint32_t uptime;            // Tempo de atividade em segundos
    uint16_t wifiRSSI;          // Força do sinal WiFi
    uint16_t sensorReadCount;   // Contagem de leituras de sensores

    // Construtor com valores padrão
    SystemStats() : freeHeap(0), minFreeHeap(0), heapFragmentation(0),
                cpuLoad(0), uptime(0), wifiRSSI(0), sensorReadCount(0) {}
};

#endif // DATA_TYPES_H
