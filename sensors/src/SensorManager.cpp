/**
 * @file SensorManager.cpp
 * @brief Implementação do gerenciador de sensores.
 */

#include "SensorManager.h"
#include "LogSystem.h"
#include "OutputManager.h"
#include "TelemetryBuffer.h"
#include "SystemMonitor.h"
#include "WiFiManager.h"
#include "StringUtils.h"

// Define o nome do módulo para logging
#define MODULE_NAME "SensorManager"

SensorManager::SensorManager()
    : m_lastReadTime(0),
    m_lastStateCheckTime(0),
    m_readCount(0),
    m_filterIndex(0) {

    // Inicializa arrays de filtro
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        m_moistureReadings[i] = 0;
    }

    // Inicializa o estado dos dados processados
    m_processedData.temperature = 25.0f; // Valor padrão razoável para temperatura
    m_processedData.humidityPercent = 50.0f; // Valor padrão razoável para umidade

    // Inicializa os dados brutos
    m_rawData.temperatureRaw = 25.0f; // Valor padrão razoável
    m_rawData.humidityRaw = 50.0f; // Valor padrão razoável
}

bool SensorManager::init() {
    LOG_INFO(MODULE_NAME, "Inicializando Gerenciador de Sensores");
    

    // Não há inicialização específica necessária para sensores neste sistema
    // Mas realizamos uma leitura inicial para popular os buffers
    readSensors();
    processSensorData();

    LOG_INFO(MODULE_NAME, "Gerenciador de sensores inicializado com sucesso");
    LOG_DEBUG(MODULE_NAME, "Buffer de filtro: %u amostras", FILTER_SIZE);

    return true;
}

uint16_t SensorManager::applyFilter(uint16_t readings[], uint16_t newValue) {
    // Atualiza o buffer circular
    readings[m_filterIndex] = newValue;

    // Calcula a média
    uint32_t sum = 0;
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += readings[i];
    }

    return static_cast<uint16_t>(sum / FILTER_SIZE);
}

float SensorManager::applyFilter(float readings[], float newValue) {
    // Implementação para valores de ponto flutuante (DHT22)

    // Atualiza o buffer circular com o novo valor
    readings[m_filterIndex] = newValue;

    // Calcula a média dos valores no buffer
    float sum = 0.0f;
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += readings[i];
    }

    return sum / FILTER_SIZE;
}

void SensorManager::readSensors() {
    // Atualiza contador
    m_readCount++;

    // Obtém timestamp atual
    m_rawData.timestamp = millis();

    // Lê o sensor DHT22 para temperatura e umidade
    float temperature = Hardware::readTemperature();
    float humidity = Hardware::readHumidity();
    
    // Aplica filtro de média móvel à temperatura e umidade do DHT22
    // para suavizar flutuações em leituras consecutivas
    static float tempBuffer[FILTER_SIZE] = {0.0f};
    static float humidityBuffer[FILTER_SIZE] = {0.0f};

    // Somente aplica o filtro se as leituras forem válidas
    if (temperature > -50.0f && temperature < 100.0f) {
        m_rawData.temperatureRaw = applyFilter(tempBuffer, temperature);
    } else {
        m_rawData.temperatureRaw = temperature; // Mantém o valor mesmo sendo inválido
    }

    if (humidity >= 0.0f && humidity <= 100.0f) {
        m_rawData.humidityRaw = applyFilter(humidityBuffer, humidity);
    } else {
        m_rawData.humidityRaw = humidity; // Mantém o valor mesmo sendo inválido
    }

    // Avança o índice do filtro
    m_filterIndex = (m_filterIndex + 1) % FILTER_SIZE;

    // Atualiza timestamp da última leitura
    m_lastReadTime = m_rawData.timestamp;

    // Leituras serão exibidas de forma centralizada no método update()
    // com técnica de atualização da mesma linha
}

void SensorManager::processSensorData() {
    // Converte dados brutos para unidades físicas
    m_processedData.fromRaw(m_rawData);

    // Leituras processadas serão exibidas de forma centralizada em update()
    // usando técnica de atualização na mesma linha
}

TelemetryBuffer SensorManager::prepareTelemetry() {
    // Prepara buffer de telemetria com dados atuais
    TelemetryBuffer telemetry;

    // Preenche com dados dos sensores
    telemetry.temperature = m_processedData.temperature;
    telemetry.humidity = m_processedData.humidityPercent;

    // Preenche estatísticas do sistema
    SystemStats stats = SystemMonitor::getInstance().getStats();
    telemetry.freeHeap = stats.freeHeap;
    telemetry.heapFragmentation = stats.heapFragmentation;
    telemetry.uptime = stats.uptime;

    // Preenche dados de WiFi
    WiFiManager& wifiManager = WiFiManager::getInstance();
    telemetry.wifiRssi = wifiManager.getRSSI();

    // Converte o IP para string
    char ipStr[16];
    IPAddress ip = wifiManager.getIP();
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    // Copia para o buffer de telemetria
    StringUtils::safeCopyString(telemetry.ipAddress, ipStr, sizeof(telemetry.ipAddress));

    // Preenche metadados
    telemetry.timestamp = millis();
    telemetry.readCount = m_readCount;

    // Retorna o buffer de telemetria para que o AsyncSoilWebServer
    // possa enviá-lo no momento apropriado
    return telemetry;
}

bool SensorManager::update(bool forceUpdate) {
    uint32_t currentTime = millis();
    static uint32_t lastDisplayUpdate = 0;
    bool dataChanged = false;

    // Verifica se é hora de atualizar
    bool timeToUpdate = (currentTime - m_lastReadTime) >= SENSOR_CHECK_INTERVAL;

    if (timeToUpdate || forceUpdate) {
        // Faz a leitura dos sensores
        readSensors();

        // Processa os dados
        processSensorData();

        
        // Não fazemos mais preparação de telemetria aqui
        // A telemetria será solicitada pelo AsyncSoilWebServer quando necessário
        if (currentTime - lastDisplayUpdate >= 500) { // 2Hz é suficiente para visualização
            lastDisplayUpdate = currentTime;
        }

        // Verifica mudanças de estado
        checkStateChanges();

        dataChanged = true;
    }

    return dataChanged;
}

const SensorData &SensorManager::getData() const {
    return m_processedData;
}

const SensorRawData &SensorManager::getRawData() const {
    return m_rawData;
}

bool SensorManager::getDataJson(char *buffer, size_t size) const {
    if (!buffer || size == 0) return false;

    return m_processedData.toJsonString(buffer, size);
}

bool SensorManager::sensorChanged(uint8_t sensorType, float threshold) const {
    static SensorData lastData;
    static uint32_t lastCheck = 0;

    uint32_t currentTime = millis();

    // Atualiza a referência a cada 5 segundos
    if (currentTime - lastCheck > 5000) {
        lastData = m_processedData;
        lastCheck = currentTime;
        return false;
    }

    // Verifica mudança com base no tipo de sensor
    switch (sensorType) {

        case 0: // Umidade
            return fabs(m_processedData.humidityPercent - lastData.humidityPercent) >
                threshold;

        default:
            return false;
    }
}