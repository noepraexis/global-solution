/**
 * @file OutputManager.cpp
 * @brief Implementação do gerenciador centralizado de saída.
 */

#include "OutputManager.h"
#include "AsyncSoilWebServer.h"

// Inicialização das variáveis estáticas
AsyncSoilWebServer* OutputManager::s_webSocketServer = nullptr;
ConsoleManager* OutputManager::s_consoleManager = nullptr;
uint32_t OutputManager::s_lastUpdateTime[4][3] = {{0}};
bool OutputManager::s_initialized = false;

void OutputManager::initialize() {
    if (s_initialized) {
        return;
    }

    // Inicialização dos tempos de atualização
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            s_lastUpdateTime[i][j] = 0;
        }
    }

    s_initialized = true;
}

void OutputManager::log(const char* module, LogLevel level,
                      OutputDestination dest, const char* fmt, ...) {
    if (!s_initialized) {
        initialize();
    }

    // Verifica se deve atualizar com base no tipo e destino
    MessageType type = (level >= LogLevel::ERROR) ? MessageType::ALERT : MessageType::DEBUG;
    if (!shouldUpdate(type, dest)) {
        return;
    }

    // Formata a mensagem
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Roteia para os destinos apropriados
    if (dest == OutputDestination::CONSOLE_ONLY || dest == OutputDestination::BOTH) {
        routeToConsole(module, level, buffer);
    }

    if (dest == OutputDestination::MEMORY_ONLY || dest == OutputDestination::BOTH) {
        routeToMemory(module, level, buffer);
    }
}

void OutputManager::telemetry(const char* sensor, TelemetryBuffer& data) {
    if (!s_initialized) {
        initialize();
    }

    // Verifica se deve atualizar
    if (!shouldUpdate(MessageType::TELEMETRY, OutputDestination::WEBSOCKET_ONLY)) {
        return;
    }

    // Encaminha para o servidor WebSocket
    routeToWebSocket(sensor, data);
}

bool OutputManager::shouldUpdate(MessageType type, OutputDestination dest) {
    uint32_t currentTime = millis();
    int destIndex = static_cast<int>(dest);
    int typeIndex = static_cast<int>(type);

    // Limites de taxa para diferentes tipos de mensagens e destinos
    const uint32_t updateIntervals[4][3] = {
        { 1000, 0, 1000 },    // DEBUG:     1Hz para console, N/A para websocket, 1Hz para memória
        { 500, 500, 1000 },   // STATUS:    2Hz para console, 2Hz para websocket, 1Hz para memória
        { 0, 100, 1000 },     // TELEMETRY: N/A para console, 10Hz para websocket, 1Hz para memória
        { 0, 0, 0 }           // ALERT:     Sem limite para nenhum destino
    };

    // Alertas críticos sempre são atualizados
    if (type == MessageType::ALERT) {
        return true;
    }

    // Verifica o intervalo para este tipo e destino
    uint32_t interval = updateIntervals[typeIndex][destIndex];
    if (interval == 0) {
        return false; // Destino não suportado para este tipo
    }

    // Verifica se passou tempo suficiente desde a última atualização
    if (currentTime - s_lastUpdateTime[typeIndex][destIndex] >= interval) {
        s_lastUpdateTime[typeIndex][destIndex] = currentTime;
        return true;
    }

    return false;
}

void OutputManager::routeToConsole(const char* module, LogLevel level, const char* message) {
    if (!s_consoleManager) {
        // Fallback para Serial se o ConsoleManager não estiver disponível
        Serial.printf("[%s] %s\n", module, message);
        return;
    }

    // Mapeia o nível de log para prioridade do console
    MessagePriority priority;
    switch (level) {
        case LogLevel::TRACE:
        case LogLevel::DEBUG:
            priority = MessagePriority::MSG_LOW;
            break;
        case LogLevel::INFO:
        case LogLevel::WARN:
            priority = MessagePriority::MSG_NORMAL;
            break;
        case LogLevel::ERROR:
            priority = MessagePriority::MSG_HIGH;
            break;
        case LogLevel::FATAL:
            priority = MessagePriority::MSG_CRITICAL;
            break;
        default:
            priority = MessagePriority::MSG_NORMAL;
    }

    // Formata a mensagem com o módulo
    char formattedMessage[320];
    snprintf(formattedMessage, sizeof(formattedMessage), "[%s] %s", module, message);

    // Envia para o console com a prioridade apropriada
    s_consoleManager->println(formattedMessage, priority);
}

void OutputManager::routeToWebSocket(const char* sensor, TelemetryBuffer& data) {
    if (!s_webSocketServer) {
        return;
    }

    // Cria documento JSON para a telemetria
    StaticJsonDocument<512> doc;

    // Criamos os objetos principais que a página web espera
    JsonObject root = doc.to<JsonObject>();

    // Criamos objetos para sensores e estatísticas
    JsonObject sensors = root.createNestedObject("sensors");
    JsonObject stats = root.createNestedObject("stats");

    // Garantir que todos os campos necessários para a UI existam,
    // mesmo que sejam zerados - isto evita o efeito de "zeragem"

    // Alimentamos os dados de sensores
    sensors["temperature"] = data.temperature;
    sensors["humidity"] = data.humidity;
    sensors["timestamp"] = data.timestamp;
    sensors["readCount"] = data.readCount;

    // Adicionamos os dados do sistema de irrigação
    JsonObject irrigation = root.createNestedObject("irrigation");
    irrigation["active"] = data.irrigationActive;
    irrigation["uptime"] = data.irrigationUptime;
    irrigation["lastActivation"] = data.lastIrrigationTime;
    irrigation["activations"] = data.dailyActivations;
    irrigation["threshold"] = data.moistureThreshold;

    // Alimentamos as estatísticas do sistema
    stats["freeHeap"] = data.freeHeap;
    stats["fragmentation"] = data.heapFragmentation;
    stats["uptime"] = data.uptime;
    stats["wifiRssi"] = data.wifiRssi;

    // Adicionar mais informações para a interface web
    stats["wifi"] = String(data.wifiRssi) + " dBm";
    stats["ipAddress"] = data.ipAddress;

    // A página web está buscando 'clients' - uma contagem de clientes
    stats["clients"] = s_webSocketServer->getClientCount();

    // Adiciona metadados
    root["source"] = sensor;
    root["timestamp"] = data.timestamp;

    // Serializa para string
    String jsonString;
    serializeJson(doc, jsonString);

    // Agora que temos um único ponto de envio,
    // todas as mensagens terão o mesmo formato completo
    s_webSocketServer->broadcastMessage(jsonString);
}

void OutputManager::routeToMemory(const char* module, LogLevel level, const char* message) {
    // Implementação de armazenamento de logs em buffer circular de memória
    LogRouter::getInstance().log(level, module, "%s", message);
}

void OutputManager::attachWebSocketServer(AsyncSoilWebServer* server) {
    s_webSocketServer = server;
}

void OutputManager::attachConsoleManager(ConsoleManager* console) {
    s_consoleManager = console;
}