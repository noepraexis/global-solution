/**
 * @file SensorManager.h
 * @brief Gerencia a leitura dos sensores de forma eficiente.
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "DataTypes.h"
#include "Hardware.h"
#include "MemoryManager.h"
#include "TelemetryBuffer.h"
#include "IrrigationController.h"

/**
 * Gerenciador de sensores
 *
 * Coordena a leitura de todos os sensores do sistema, implementando
 * técnicas de otimização como leitura com debounce, filtragem e
 * monitoramento de mudanças de estado.
 */
class SensorManager {
private:
    SensorRawData m_rawData;
    SensorData m_processedData;

    // Controle de tempo
    uint32_t m_lastReadTime;
    uint32_t m_lastStateCheckTime;

    // Contadores
    uint16_t m_readCount;

    // Filtro de média móvel para sensores analógicos
    static constexpr uint8_t FILTER_SIZE = 5;
    uint16_t m_moistureReadings[FILTER_SIZE];
    uint8_t m_filterIndex;

    /**
     * Aplica um filtro de média móvel às leituras analógicas.
     *
     * @param readings Array de leituras históricas.
     * @param newValue Novo valor a ser adicionado.
     * @return Média dos valores no buffer.
     */
    uint16_t applyFilter(uint16_t readings[], uint16_t newValue);

    /**
     * Aplica um filtro de média móvel às leituras de ponto flutuante do DHT22.
     *
     * @param readings Array de leituras históricas.
     * @param newValue Novo valor a ser adicionado.
     * @return Média dos valores no buffer.
     */
    float applyFilter(float readings[], float newValue);

    /**
     * Verifica mudanças nos sensores digitais e gera eventos.
     */
    void checkStateChanges();

    /**
     * Realiza as leituras dos sensores.
     */
    void readSensors();

    /**
     * Processa os dados brutos para unidades físicas.
     */
    void processSensorData();

    // Movido para public para acesso em outras classes

public:
    /**
     * Construtor do gerenciador de sensores.
     */
    SensorManager();

    /**
     * Inicializa o gerenciador de sensores.
     *
     * @return true se a inicialização for bem-sucedida.
     */
    bool init();

    /**
     * Atualiza os sensores se o intervalo adequado tiver passado.
     *
     * @param forceUpdate Force atualização mesmo sem intervalo.
     * @return true se os sensores foram atualizados.
     */
    bool update(bool forceUpdate = false);

    /**
     * Obtém os dados atuais dos sensores.
     *
     * @return Referência para os dados processados.
     */
    const SensorData &getData() const;

    /**
     * Obtém os dados brutos dos sensores.
     *
     * @return Referência para os dados brutos.
     */
    const SensorRawData &getRawData() const;

    /**
     * Obtém os dados em formato JSON.
     *
     * @param buffer Buffer para armazenar o JSON.
     * @param size Tamanho do buffer.
     * @return true se a conversão foi bem-sucedida.
     */
    bool getDataJson(char *buffer, size_t size) const;

    /**
     * Verifica se um sensor específico mudou de estado.
     *
     * @param sensorType Tipo de sensor (0=PH, 1=Moisture, 2=Phosphorus, 3=Potassium)
     * @param threshold Limiar para considerar mudança (para sensores analógicos)
     * @return true se o sensor mudou além do threshold.
     */
    bool sensorChanged(uint8_t sensorType, float threshold = 0.5f) const;

    /**
     * Prepara dados de telemetria.
     *
     * Coleta e formata dados de sensores e sistema, retornando
     * um buffer preenchido para ser enviado por outro componente.
     *
     * @return Buffer de telemetria preenchido com dados atuais
     */
    TelemetryBuffer prepareTelemetry();

    /**
     * Ativa irrigação manualmente.
     *
     * @param duration Duração em milissegundos (0 = até desativar manualmente).
     * @return true se a ativação foi bem-sucedida.
     */
    bool activateIrrigation(uint32_t duration = 0);

    /**
     * Desativa irrigação.
     *
     * @param manual true se foi desativação manual.
     * @return true se a desativação foi bem-sucedida.
     */
    bool deactivateIrrigation(bool manual = true);

    /**
     * Verifica se a irrigação está ativa.
     *
     * @return true se a bomba está ligada.
     */
    bool isIrrigationActive() const;

    /**
     * Obtém dados do sistema de irrigação.
     *
     * @return Referência constante aos dados de irrigação.
     */
    const IrrigationData& getIrrigationData() const;

    /**
     * Processa comando de irrigação via WebSocket.
     *
     * @param activate true para ativar, false para desativar.
     * @param duration Duração em milissegundos (apenas para ativação).
     * @return true se o comando foi processado com sucesso.
     */
    bool processIrrigationCommand(bool activate, uint32_t duration = 0);
};

#endif // SENSOR_MANAGER_H
