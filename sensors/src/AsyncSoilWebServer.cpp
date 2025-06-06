/**
 * @file AsyncSoilWebServer.cpp
 * @brief Implementação do servidor web assíncrono.
 */

#include "AsyncSoilWebServer.h"
#include "LogSystem.h"
#include "OutputManager.h"
#include "TelemetryBuffer.h"

// Nome do módulo para logs
#define MODULE_NAME "WebServer"

// HTML da página principal (armazenado em PROGMEM para economizar RAM)
// Com sistema de atualização inteligente para evitar flickering
const char AsyncSoilWebServer::INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sistema de Monitoramento de Solo</title>
    <style>
    body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        margin: 0;
        padding: 20px;
        background-color: #f5f5f5;
        color: #333;
    }
    h1 {
        color: #2c3e50;
        text-align: center;
        margin-bottom: 20px;
    }
    .container {
        display: flex;
        flex-wrap: wrap;
        gap: 20px;
        justify-content: center;
    }
    .box {
        background-color: white;
        border-radius: 10px;
        padding: 20px;
        box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
        min-width: 200px;
        flex: 1;
    }
    h2 {
        margin-top: 0;
        margin-bottom: 15px;
        font-size: 1.2em;
        color: #3498db;
    }
    .value {
        font-size: 2em;
        font-weight: bold;
        text-align: center;
        margin: 10px 0;
    }
    .stats {
        font-size: 1em;
        line-height: 1.6;
    }
    .scale {
        height: 20px;
        background: linear-gradient(to right, red, yellow, green, blue, purple);
        border-radius: 10px;
        position: relative;
        margin: 10px 0;
    }
    .marker {
        width: 10px;
        height: 25px;
        background-color: #2c3e50;
        position: absolute;
        top: -2px;
        transform: translateX(-50%);
        border-radius: 5px;
    }
    .status {
        padding: 5px 10px;
        border-radius: 5px;
        font-size: 0.8em;
        text-align: center;
    }
    .on {
        background-color: #27ae60;
        color: white;
    }
    .off {
        background-color: #e74c3c;
        color: white;
    }
    @media (max-width: 768px) {
        .container {
            flex-direction: column;
        }
        .box {
            min-width: auto;
        }
    }
    </style>
</head>
<body>
    <h1>Sistema de Monitoramento de Solo</h1>
    <div class="container">
        <div class="box">
            <h2>Temperatura</h2>
            <div class="value" id="temperature-value">0.0°C</div>
        </div>
        <div class="box">
            <h2>Umidade do Ar</h2>
            <div class="value" id="humidity-value">0.0%</div>
        </div>
        <div class="box">
            <h2>Sistema de Irrigação</h2>
            <div class="value">
                <span id="pump-status" class="status off">DESLIGADA</span>
            </div>
            <button id="pump-toggle" onclick="togglePump()" style="margin: 10px 0; padding: 10px 20px; border: none; border-radius: 5px; background-color: #3498db; color: white; cursor: pointer;">Alternar Bomba</button>
            <div class="stats">
                <div>Tempo funcionamento: <span id="pump-runtime">0</span>s</div>
                <div>Ativações hoje: <span id="pump-activations">0</span></div>
                <div>Limiar umidade: <span id="moisture-threshold">30.0</span>%</div>
            </div>
        </div>
    </div>

    <div class="container" style="margin-top: 20px;">
        <div class="box" style="width: 100%;">
            <h2>Estatísticas do Sistema</h2>
            <div class="stats">
                <div>Memória livre: <span id="free-memory">0</span> bytes</div>
                <div>Fragmentação: <span id="fragmentation">0</span>%</div>
                <div>Tempo ativo: <span id="uptime">0</span> segundos</div>
                <div>Clientes conectados: <span id="clients">0</span></div>
                <div>WiFi: <span id="wifi-status">Desconectado</span></div>
            </div>
        </div>
    </div>

    <script>
    // Armazena valores atuais para evitar atualizações desnecessárias
    const currentValues = {
        'temperature-value': '0.0°C',
        'humidity-value': '0.0%',
        'free-memory': '0',
        'fragmentation': '0%',
        'uptime': '0',
        'clients': '0',
        'wifi-status': 'Desconectado',
        'pump-status': { text: 'DESLIGADA', className: 'status off' },
        'pump-runtime': '0',
        'pump-activations': '0'
    };

    // Função para atualizar elemento apenas se o valor for diferente
    function updateElementIfChanged(id, newValue, isSpecial = false) {
        const element = document.getElementById(id);
        if (!element) return false;

        if (isSpecial) {
            // Para elementos com múltiplas propriedades (como status)
            if (currentValues[id].text !== newValue.text ||
                currentValues[id].className !== newValue.className) {
                element.textContent = newValue.text;
                element.className = newValue.className;
                currentValues[id] = { ...newValue };
                return true;
            }
        } else {
            // Para elementos de texto simples
            if (currentValues[id] !== newValue) {
                element.textContent = newValue;
                currentValues[id] = newValue;
                return true;
            }
        }
        return false;
    }

    // WebSocket
    let ws = null;
    let reconnectInterval = 1000;
    let reconnectAttempts = 0;
    const maxReconnectAttempts = 10;

    function connectWebSocket() {
        if (ws) {
            ws.close();
        }

        // Usar protocolo atual (http->ws, https->wss)
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        ws = new WebSocket(wsUrl);

        ws.onopen = function() {
            console.log('WebSocket conectado');
            reconnectInterval = 1000; // Reseta intervalo
            reconnectAttempts = 0;    // Reseta contador
        };

        ws.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                updateUI(data);
            } catch (e) {
                console.error('Erro ao analisar dados:', e);
            }
        };

        ws.onclose = function() {
            console.log('WebSocket desconectado');

            // Reconecta com backoff exponencial
            if (reconnectAttempts < maxReconnectAttempts) {
                setTimeout(function() {
                    reconnectAttempts++;
                    reconnectInterval *= 1.5; // Backoff exponencial
                    connectWebSocket();
                }, reconnectInterval);
            }
        };

        ws.onerror = function(error) {
            console.error('Erro WebSocket:', error);
            ws.close(); // Fechará e acionará onclose para reconexão
        };
    }

    function updateUI(data) {
        // Atualiza sensores
        if (data.sensors) {
            // Temperatura
            if (typeof data.sensors.temperature === 'number') {
                updateElementIfChanged('temperature-value', data.sensors.temperature.toFixed(1) + '°C');
            }

            // Umidade do ar
            if (typeof data.sensors.humidity === 'number') {
                updateElementIfChanged('humidity-value', data.sensors.humidity.toFixed(1) + '%');
            }
        }

        // Atualiza irrigação
        if (data.irrigation) {
            const pumpStatus = document.getElementById('pump-status');
            const pumpToggle = document.getElementById('pump-toggle');

            if (data.irrigation.active) {
                updateElementIfChanged('pump-status', {
                    text: 'LIGADA',
                    className: 'status on'
                }, true);
                if (pumpToggle) pumpToggle.textContent = 'Desligar Bomba';
            } else {
                updateElementIfChanged('pump-status', {
                    text: 'DESLIGADA',
                    className: 'status off'
                }, true);
                if (pumpToggle) pumpToggle.textContent = 'Ligar Bomba';
            }

            if (data.irrigation.uptime !== undefined) {
                updateElementIfChanged('pump-runtime', data.irrigation.uptime.toString());
            }

            if (data.irrigation.dailyActivations !== undefined) {
                updateElementIfChanged('pump-activations', data.irrigation.dailyActivations.toString());
            }
        }

        // Atualiza estatísticas
        if (data.stats) {
            // Memória livre
            if (data.stats.freeHeap !== undefined) {
                updateElementIfChanged('free-memory', data.stats.freeHeap.toString());
            }

            // Fragmentação
            if (data.stats.fragmentation !== undefined) {
                updateElementIfChanged('fragmentation', data.stats.fragmentation);
            }

            // Tempo de atividade formatado
            if (data.stats.uptime !== undefined) {
                const uptime = data.stats.uptime;
                const days = Math.floor(uptime / 86400);
                const hours = Math.floor((uptime % 86400) / 3600);
                const minutes = Math.floor((uptime % 3600) / 60);
                const seconds = uptime % 60;
                const uptimeFormatted =
                    (days > 0 ? days + 'd ' : '') +
                    (hours > 0 ? hours + 'h ' : '') +
                    (minutes > 0 ? minutes + 'm ' : '') +
                    seconds + 's';

                updateElementIfChanged('uptime', uptimeFormatted);
            }

            // Clientes conectados
            if (data.stats.clients !== undefined) {
                updateElementIfChanged('clients', data.stats.clients.toString());
            }

            // Estado do WiFi
            if (data.stats.wifi !== undefined) {
                updateElementIfChanged('wifi-status', data.stats.wifi);
            }
        }
    }

    function togglePump() {
        try {
            if (ws && ws.readyState === WebSocket.OPEN) {
                const command = {
                    action: 'irrigation_toggle'
                };
                ws.send(JSON.stringify(command));
                console.log('Comando de irrigação enviado');
            } else {
                console.error('WebSocket não conectado');
                alert('Conexão perdida. Recarregue a página.');
            }
        } catch (error) {
            console.error('Erro ao alternar bomba:', error);
            alert('Erro ao enviar comando. Tente novamente.');
        }
    }

    // Conexão inicial
    document.addEventListener('DOMContentLoaded', function() {
        connectWebSocket();

        // Fallback: se WebSocket falhar, faz polling
        setInterval(function() {
            if (ws && ws.readyState !== WebSocket.OPEN) {
                fetch('/data')
                    .then(response => response.json())
                    .then(data => updateUI(data))
                    .catch(error => console.error('Erro na API:', error));
            }
        }, 2000);
    });
    </script>
</body>
</html>)rawliteral";

// Armazena um ponteiro para a instância que está sendo usada
// Uma vez que a biblioteca não fornece meios de associar o ponteiro this ao websocket
static AsyncSoilWebServer* s_instance = nullptr;

AsyncSoilWebServer::AsyncSoilWebServer(uint16_t port, SensorManager &sensorManager)
    : m_server(port),
    m_websocket("/ws"),
    m_sensorManager(sensorManager),
    m_lastBroadcastTime(0),
    m_clientCount(0),
    m_broadcastCount(0) {
}

bool AsyncSoilWebServer::begin() {
    // Armazena a instância atual na variável estática
    s_instance = this;

    // Configura o handler de eventos WebSocket
    m_websocket.onEvent(onWebSocketEvent);

    // Adiciona o handler WebSocket ao servidor
    m_server.addHandler(&m_websocket);

    // Configura as rotas do servidor
    m_server.on("/", HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleRoot(request); });

    m_server.on("/data", HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleData(request); });

    // Rota para obter o histórico de logs do sistema
    m_server.on("/logs", HTTP_GET,
        [this](AsyncWebServerRequest *request) { handleLogs(request); });

    // Handler para rotas não encontradas
    m_server.onNotFound(
        [this](AsyncWebServerRequest *request) { handleNotFound(request); });

    // Inicia o servidor
    m_server.begin();

    LOG_INFO(MODULE_NAME, "Servidor iniciado na porta %u", WEB_SERVER_PORT);
    LOG_INFO(MODULE_NAME, "Acesse http://127.0.0.1:8888 no navegador");

    return true;
}

void AsyncSoilWebServer::handleRoot(AsyncWebServerRequest *request) {
    // Envia a página HTML
    request->send(200, "text/html", INDEX_HTML);

    if (DEBUG_MODE) {
        {
            IPAddress ip = request->client()->remoteIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DBG_DEBUG(MODULE_NAME, "Página principal requisitada por %s", ipStr);
        }
    }
}

void AsyncSoilWebServer::handleData(AsyncWebServerRequest *request) {
    // Força atualização dos sensores
    m_sensorManager.update(true);

    // Agora que a telemetria está centralizada no OutputManager,
    // usamos a rota prepareTelemetry e enviamos ao cliente HTTP
    TelemetryBuffer telemetry = m_sensorManager.prepareTelemetry();

    // Cria documento JSON usando o mesmo formato que usamos para WebSocket
    StaticJsonDocument<512> doc;
    JsonObject root = doc.to<JsonObject>();
    JsonObject sensors = root.createNestedObject("sensors");

    // Dados de sensores (suficiente para a API /data)
    sensors["temperature"] = telemetry.temperature;
    sensors["humidity"] = telemetry.humidity;
    sensors["timestamp"] = telemetry.timestamp;
    sensors["readCount"] = telemetry.readCount;

    // Serializa para string
    String response;
    serializeJson(doc, response);

    // Envia a resposta
    request->send(200, "application/json", response);

    if (DEBUG_MODE) {
        {
            IPAddress ip = request->client()->remoteIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DBG_DEBUG(MODULE_NAME, "API dados requisitada por %s", ipStr);
        }
    }
}

void AsyncSoilWebServer::handleLogs(AsyncWebServerRequest *request) {
    // Tamanho máximo do buffer para armazenar os logs
    const size_t bufferSize = LOG_BUFFER_SIZE * (LOG_MAX_MESSAGE_SIZE + 64);

    // Aloca buffer para armazenar os logs
    char* logBuffer = new char[bufferSize];
    if (!logBuffer) {
        // Caso não consiga alocar memória, retorna erro
        LOG_ERROR(MODULE_NAME, "Falha ao alocar memória para buffer de logs");
        request->send(500, "application/json", "{\"error\":\"Memória insuficiente\"}");
        return;
    }

    // Inicializa o buffer
    memset(logBuffer, 0, bufferSize);

    // Obtém os logs do buffer circular
    size_t bytesWritten = LogRouter::getInstance().getStoredLogs(logBuffer, bufferSize - 1);

    if (bytesWritten == 0) {
        // Se não há logs, retorna array vazio
        delete[] logBuffer;
        request->send(200, "application/json", "{\"logs\":[]}");
        return;
    }

    // Verifica o parâmetro de formato - padrão é JSON
    String format = request->hasParam("format") ? request->getParam("format")->value() : "json";

    if (format.equalsIgnoreCase("text") || format.equalsIgnoreCase("plain")) {
        // Retorna como texto simples
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", logBuffer);
        request->send(response);
    } else {
        // Formata como JSON
        StaticJsonDocument<512> doc;
        JsonArray logs = doc.createNestedArray("logs");

        // Converte as linhas de log em um array JSON
        char* line = strtok(logBuffer, "\n");
        while (line != nullptr) {
            if (strlen(line) > 0) {
                logs.add(line);
            }
            line = strtok(nullptr, "\n");
        }

        // Serializa o documento JSON
        String jsonResponse;
        serializeJson(doc, jsonResponse);

        // Envia a resposta
        request->send(200, "application/json", jsonResponse);
    }

    // Libera o buffer
    delete[] logBuffer;

    if (DEBUG_MODE) {
        {
            IPAddress ip = request->client()->remoteIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DBG_DEBUG(MODULE_NAME, "API logs requisitada por %s (formato: %s)",
                ipStr, format.c_str());
        }
    }
}

void AsyncSoilWebServer::handleNotFound(AsyncWebServerRequest *request) {
    // Envia resposta 404
    request->send(404, "text/plain", "Página não encontrada");

    if (DEBUG_MODE) {
        {
            IPAddress ip = request->client()->remoteIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DBG_DEBUG(MODULE_NAME, "404 - %s (IP: %s)",
                request->url().c_str(), ipStr);
        }
    }
}

String AsyncSoilWebServer::prepareJsonMessage(bool dataOnly) {
    // Aloca documento JSON na stack
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;

    // Dados dos sensores
    JsonObject sensors = doc.createNestedObject("sensors");
    const SensorData& data = m_sensorManager.getData();

    // Agora a temperatura já vem calibrada diretamente do sensor
    // A calibração é aplicada em Hardware::readTemperature() através da função
    // Hardware::getCalibrationTemperature()
    sensors["temperature"] = data.temperature;
    sensors["humidity"] = data.humidityPercent;
    sensors["timestamp"] = data.timestamp;

    // Adiciona estatísticas do sistema se solicitado
    if (!dataOnly) {
        // Obtém estatísticas atualizadas
        const SystemStats& stats = SystemMonitor::getInstance().getStats();

        JsonObject statsObj = doc.createNestedObject("stats");
        statsObj["freeHeap"] = stats.freeHeap;
        statsObj["fragmentation"] = stats.heapFragmentation;
        statsObj["uptime"] = stats.uptime;
        statsObj["clients"] = m_clientCount;

        // Status WiFi
        char wifiStatus[50];
        WiFiManager::getInstance().getStatusString(wifiStatus, sizeof(wifiStatus));
        statsObj["wifi"] = wifiStatus;
    }

    // Serializa para string
    String output;
    serializeJson(doc, output);

    return output;
}

bool AsyncSoilWebServer::update(bool forceUpdate) {
    uint32_t currentTime = millis();

    // Atualiza apenas a cada 100ms para limitar carga de rede (10Hz)
    if (forceUpdate || (currentTime - m_lastBroadcastTime >= 100)) {
        // Limpa clientes inativos a cada 5 segundos
        if (currentTime % 5000 < 100) {
            cleanClients();
        }

        // Apenas solicita atualização se houver clientes conectados
        if (m_clientCount > 0) {
            // Atualiza as estatísticas do sistema
            SystemMonitor::getInstance().update();

            // Solicita que o SensorManager atualize seus dados
            m_sensorManager.update(forceUpdate);

            {
                // Escopo local para variável telemetry
                TelemetryBuffer telemetry = m_sensorManager.prepareTelemetry();

                // Envia telemetria diretamente pelo WebSocket (centralizado)
                TELEMETRY(MODULE_NAME, telemetry);
            }

            // Atualiza timestamp e contador
            m_lastBroadcastTime = currentTime;
            m_broadcastCount++;

            if (DEBUG_MODE && m_broadcastCount % 100 == 0) { // Log apenas a cada 100 broadcasts
                DBG_DEBUG(MODULE_NAME, "Dados enviados para %u clientes (envio #%u)",
                    m_clientCount, m_broadcastCount);
            }

            return true;
        }
    }

    return false;
}

uint16_t AsyncSoilWebServer::getClientCount() const {
    return m_clientCount;
}

bool AsyncSoilWebServer::broadcastMessage(const String &message) {
    // Envia a mensagem para todos os clientes
    m_websocket.textAll(message);

    // Retorna true se há clientes para receber
    return (m_clientCount > 0);
}

uint16_t AsyncSoilWebServer::cleanClients() {
    // Limpa clientes inativos
    uint16_t initialCount = m_clientCount;
    m_websocket.cleanupClients();

    // Atualiza contagem de clientes
    m_clientCount = m_websocket.count();

    uint16_t removedCount = initialCount - m_clientCount;
    if (removedCount > 0 && DEBUG_MODE) {
        DBG_DEBUG(MODULE_NAME, "WebSocket: %u clientes inativos removidos", removedCount);
    }

    return removedCount;
}

void AsyncSoilWebServer::handleWebSocketEvent(AsyncWebSocket *server,
                                AsyncWebSocketClient *client,
                                AwsEventType type, void *arg, uint8_t *data,
                                size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            // Novo cliente conectado
            m_clientCount++;
            if (DEBUG_MODE) {
                DBG_DEBUG(MODULE_NAME, "WebSocket: Cliente #%u conectado", client->id());
            }

            // Atualiza estatísticas do sistema antes de enviar
            SystemMonitor::getInstance().update();

            // Força uma atualização dos sensores para ter valores mais atuais
            m_sensorManager.update(true);

            {
                // Escopo local para variável telemetry para evitar erro de salto sobre inicialização
                TelemetryBuffer telemetry = m_sensorManager.prepareTelemetry();
                TELEMETRY(MODULE_NAME, telemetry);
            }
            break;

        case WS_EVT_DISCONNECT:
            // Cliente desconectado
            m_clientCount--;
            if (DEBUG_MODE) {
                DBG_DEBUG(MODULE_NAME, "WebSocket: Cliente #%u desconectado", client->id());
            }
            break;

        case WS_EVT_DATA:
            // Dados recebidos do cliente
            {
                AwsFrameInfo *info = (AwsFrameInfo *)arg;
                if (info->final && info->index == 0 && info->len == len) {
                    // Mensagem completa recebida
                    if (DEBUG_MODE) {
                        DBG_DEBUG(MODULE_NAME, "WebSocket: Recebido %u bytes do cliente #%u",
                                len, client->id());
                    }

                    // Processa comandos JSON do cliente
                    processWebSocketCommand(client, data, len);
                }
            }
            break;

        case WS_EVT_PONG:
        case WS_EVT_PING:
        case WS_EVT_ERROR:
            // Sem processamento especial para PONG, PING ou ERROR
            break;
    }
}

void AsyncSoilWebServer::processWebSocketCommand(AsyncWebSocketClient *client,
                                               uint8_t *data, size_t len) {
    // Converte os dados recebidos em string
    char* commandStr = new char[len + 1];
    if (!commandStr) {
        LOG_ERROR(MODULE_NAME, "Falha ao alocar memória para comando WebSocket");
        return;
    }

    memcpy(commandStr, data, len);
    commandStr[len] = '\0';

    // Analisa o comando JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, commandStr);

    if (error) {
        LOG_WARN(MODULE_NAME, "Comando JSON inválido recebido: %s", error.c_str());
        delete[] commandStr;
        return;
    }

    // Processa o comando baseado na ação
    const char* action = doc["action"];
    if (action) {
        if (strcmp(action, "irrigation_toggle") == 0) {
            // Comando para alternar irrigação
            // Código correto (TOGGLE):
            bool isActive = m_sensorManager.isIrrigationActive();
            bool success = isActive ?
                m_sensorManager.deactivateIrrigation(true) :  // Se ativa, desativa manual
                m_sensorManager.activateIrrigation();          // Se inativa, ativa

            if (DEBUG_MODE) {
                DBG_DEBUG(MODULE_NAME, "Comando irrigação do cliente #%u: %s",
                        client->id(), success ? "sucesso" : "falhou");
            }

            // Envia confirmação de volta ao cliente
            StaticJsonDocument<128> response;
            response["type"] = "irrigation_response";
            response["success"] = success;
            response["action"] = "toggle";

            String responseStr;
            serializeJson(response, responseStr);
            client->text(responseStr);

            // Força atualização imediata dos dados para todos os clientes
            if (success) {
                m_sensorManager.update(true);
                TelemetryBuffer telemetry = m_sensorManager.prepareTelemetry();
                TELEMETRY(MODULE_NAME, telemetry);
            }
        }
        else {
            LOG_WARN(MODULE_NAME, "Ação desconhecida recebida: %s", action);
        }
    }

    delete[] commandStr;
}

void AsyncSoilWebServer::onWebSocketEvent(AsyncWebSocket *server,
                                        AsyncWebSocketClient *client,
                                        AwsEventType type, void *arg, uint8_t *data,
                                        size_t len) {
    // Usa a variável estática definida anteriormente para obter a instância
    if (s_instance) {
        s_instance->handleWebSocketEvent(server, client, type, arg, data, len);
    }
}