// src/WiFiManager.cpp
/**
 * @file WiFiManager.cpp
 * @brief Implementação do gerenciador de WiFi.
 */

#include "WiFiManager.h"
#include "ConsoleFormat.h"
#include "LogSystem.h"
#include "OutputManager.h"
#include "TelemetryBuffer.h"
#include "StringUtils.h"

// Nome do módulo para logs
static const char* MODULE_NAME = "WiFi";

// Inicializa o ponteiro da instância singleton como null
WiFiManager *WiFiManager::s_instance = nullptr;

WiFiManager::WiFiManager()
    : m_connected(false),
    m_rssi(0),
    m_reconnectTime(0),
    m_reconnectAttempts(0) {
}

WiFiManager &WiFiManager::getInstance() {
    // Se a instância ainda não foi criada, cria-a
    if (s_instance == nullptr) {
        s_instance = new WiFiManager();
    }
    return *s_instance;
}

void WiFiManager::WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    WiFiManager &instance = getInstance();

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            LOG_INFO(MODULE_NAME, "Conectado ao ponto de acesso");
            // Ainda não obteve IP neste ponto
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            LOG_INFO(MODULE_NAME, "Conexão WiFi estabelecida");

            instance.m_connected = true;
            instance.m_ipAddress = WiFi.localIP();
            instance.m_reconnectAttempts = 0;  // Reseta o contador de tentativas

            {
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                         instance.m_ipAddress[0], instance.m_ipAddress[1],
                         instance.m_ipAddress[2], instance.m_ipAddress[3]);
                LOG_INFO(MODULE_NAME, "Endereço IP: %s", ipStr);
            }
            LOG_INFO(MODULE_NAME, "Potência do sinal: %d dBm", WiFi.RSSI());

            // Liga o LED quando conectado
            Hardware::setLedState(Hardware::LED_ON);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            instance.m_connected = false;

            LOG_WARN(MODULE_NAME, "Dispositivo desconectado do ponto de acesso");

            // Programa uma tentativa de reconexão se não excedeu o limite
            if (instance.m_reconnectAttempts < WIFI_MAX_RECONNECT_ATTEMPTS) {
                instance.m_reconnectTime = millis() + WIFI_RECONNECT_INTERVAL;
                instance.m_reconnectAttempts++;

                // Pisca o LED enquanto tenta reconectar
                Hardware::toggleLed();

                LOG_INFO(MODULE_NAME, "Tentativa de reconexão em %ums (tentativa %u/%u)",
                        WIFI_RECONNECT_INTERVAL,
                        instance.m_reconnectAttempts,
                        WIFI_MAX_RECONNECT_ATTEMPTS);
            } else {
                LOG_ERROR(MODULE_NAME, "Excedeu máximo de tentativas de reconexão");
                LOG_ERROR(MODULE_NAME, "Reinicie o dispositivo para tentar novamente");

                // Desliga o LED para indicar falha
                Hardware::setLedState(Hardware::LED_OFF);
            }
            break;

        default:
            // Ignora outros eventos
            break;
    }
}

bool WiFiManager::connect(const char *ssid, const char *password) {
    // Verifica se o WiFi já foi inicializado pelo módulo de performance
    extern volatile bool g_wifiEarlyInitDone;
    extern volatile bool g_wifiEarlyInitSuccess;

    LOG_INFO(MODULE_NAME, "Iniciando conexão WiFi");

    if (g_wifiEarlyInitDone) {
        LOG_INFO(MODULE_NAME, "WiFi já inicializado pelo módulo de performance");

        // Ainda registra o handler para gerenciar reconexões e outros eventos
        // mas não reconfigura o WiFi para evitar conflitos
        WiFi.onEvent(WiFiEventHandler);

        // Se a conexão já estiver ativa, apenas atualiza o estado interno
        if (WiFi.status() == WL_CONNECTED && g_wifiEarlyInitSuccess) {
            m_connected = true;
            m_ipAddress = WiFi.localIP();
            m_reconnectAttempts = 0;
            {
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                        m_ipAddress[0], m_ipAddress[1], m_ipAddress[2], m_ipAddress[3]);
                LOG_INFO(MODULE_NAME, "Usando conexão existente com IP: %s", ipStr);
            }
            return true;
        }

        // A conexão foi tentada mas falhou, programa uma reconexão
        LOG_WARN(MODULE_NAME, "Conexão prévia falhou, programando reconexão");
        m_reconnectTime = millis() + WIFI_RECONNECT_INTERVAL;
        m_reconnectAttempts = 1;
        return false;
    }

    // Configuração normal quando não foi inicializado pelo módulo de performance
    LOG_INFO(MODULE_NAME, "Inicializando WiFi (primeira vez)");

    // Configura o modo de operação
    WiFi.mode(WIFI_STA);

    // Desativa o modo de economia de energia para menor latência
    WiFi.setSleep(false);

    // Registra handlers de eventos WiFi
    WiFi.onEvent(WiFiEventHandler);

    // Inicia a conexão
    #if defined(WOKWI_ENV) || defined(WOKWI)
        // Configuração específica para Wokwi - Canal 6 é crucial para evitar asserções
        WiFi.begin(ssid, password, 6);
        LOG_INFO(MODULE_NAME, "Conectando ao WiFi '%s' no canal 6 (Wokwi)", ssid);
    #else
        // Configuração normal para hardware real
        WiFi.begin(ssid, password);
        LOG_INFO(MODULE_NAME, "Conectando ao WiFi '%s'", ssid);
    #endif

    m_reconnectTime = millis();
    m_reconnectAttempts = 1;

    return true;
}

void WiFiManager::prepareTelemetry() {
    // Cria buffer de telemetria
    TelemetryBuffer telemetry;

    // Preenche dados de WiFi
    telemetry.wifiRssi = getRSSI();

    // Converte o IP para string
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", m_ipAddress[0], m_ipAddress[1], m_ipAddress[2], m_ipAddress[3]);

    // Copia para o buffer de telemetria
    StringUtils::safeCopyString(telemetry.ipAddress, ipStr, sizeof(telemetry.ipAddress));

    // Preenche metadados
    telemetry.timestamp = millis();

    // Envia telemetria
    TELEMETRY(MODULE_NAME, telemetry);
}

bool WiFiManager::update() {
    uint32_t currentTime = millis();
    static uint32_t lastTelemetryTime = 0;

    // Atualiza o estado da conexão
    if (WiFi.status() == WL_CONNECTED) {
        if (!m_connected) {
            m_connected = true;
            m_ipAddress = WiFi.localIP();

            {
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                        m_ipAddress[0], m_ipAddress[1], m_ipAddress[2], m_ipAddress[3]);
                LOG_INFO(MODULE_NAME, "Conexão confirmada com IP: %s", ipStr);
            }

            // Liga o LED quando conectado
            Hardware::setLedState(Hardware::LED_ON);
        }

        // Atualiza RSSI e envia telemetria a cada intervalo (500ms)
        if (currentTime - lastTelemetryTime >= 500) {
            // Atualiza o RSSI
            m_rssi = WiFi.RSSI();

            // Prepara e envia telemetria
            prepareTelemetry();

            lastTelemetryTime = currentTime;
        }
    } else {
        if (m_connected) {
            m_connected = false;

            LOG_WARN(MODULE_NAME, "Conexão perdida");

            // Pisca o LED para indicar perda de conexão
            Hardware::toggleLed();
        }

        // Tenta reconectar se for hora e não excedeu o limite
        if (m_reconnectAttempts < WIFI_MAX_RECONNECT_ATTEMPTS &&
            currentTime >= m_reconnectTime) {

            LOG_INFO(MODULE_NAME, "Tentando reconectar (tentativa %u/%u)",
                   m_reconnectAttempts + 1, WIFI_MAX_RECONNECT_ATTEMPTS);

            // Incrementa o contador e agenda próxima tentativa com backoff exponencial
            m_reconnectAttempts++;

            // Backoff exponencial: 1s, 2s, 4s, 8s, etc.
            // Limitado por segurança para evitar overflow
            unsigned int exponent = 0;
            if (m_reconnectAttempts > 1) {
                exponent = (m_reconnectAttempts - 1 > 8) ? 8 : (m_reconnectAttempts - 1);
            }

            // Calcula próximo intervalo com backoff
            uint32_t nextInterval = WIFI_RECONNECT_INTERVAL * (1U << exponent);
            m_reconnectTime = currentTime + nextInterval;

            LOG_INFO(MODULE_NAME, "Próxima tentativa em %ums se falhar", nextInterval);

            // Tenta reconectar
            WiFi.disconnect();
            delay(100);

            #if defined(WOKWI_ENV) || defined(WOKWI)
                // Reconexão específica para Wokwi - canal 6 é crucial
                LOG_INFO(MODULE_NAME, "Usando configuração específica do Wokwi (canal 6)");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
            #else
                LOG_INFO(MODULE_NAME, "Usando método padrão de reconexão");
                WiFi.reconnect();
            #endif

            // Pisca o LED para indicar tentativa
            Hardware::toggleLed();
        }
    }

    return m_connected;
}

bool WiFiManager::isConnected() const {
    return m_connected;
}

IPAddress WiFiManager::getIP() const {
    return m_ipAddress;
}

int16_t WiFiManager::getRSSI() {
    // Atualiza o RSSI apenas a cada segundo para evitar sobrecarga
    static uint32_t lastRssiUpdate = 0;
    uint32_t currentTime = millis();

    if (m_connected && (currentTime - lastRssiUpdate >= 1000)) {
        m_rssi = WiFi.RSSI();
        lastRssiUpdate = currentTime;

        // Com o novo sistema de prioridade, podemos usar opcionalmente o log
        // de baixa prioridade - será mostrado apenas quando não houver linha reservada
        #if defined(DEBUG_WIFI_RSSI) && DEBUG_WIFI_RSSI
        LOG_DEBUG(MODULE_NAME, "RSSI: %d dBm", m_rssi);
        #endif
    }

    return m_rssi;
}

char* WiFiManager::getStatusString(char *buffer, size_t size) {
    if (!buffer || size == 0) return nullptr;

    if (m_connected) {
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", m_ipAddress[0], m_ipAddress[1], m_ipAddress[2], m_ipAddress[3]);

        snprintf(buffer, size, "Conectado - IP: %s, RSSI: %d dBm",
                ipStr, getRSSI());
    } else if (m_reconnectAttempts < WIFI_MAX_RECONNECT_ATTEMPTS) {
        snprintf(buffer, size, "Desconectado - Reconectando (%u/%u)",
                m_reconnectAttempts, WIFI_MAX_RECONNECT_ATTEMPTS);
    } else {
        snprintf(buffer, size, "Desconectado - Máximo de tentativas excedido");
    }

    return buffer;
}

void WiFiManager::disconnect() {
    if (m_connected) {
        LOG_INFO(MODULE_NAME, "Desconectando manualmente do WiFi");

        WiFi.disconnect();
        m_connected = false;

        // Desliga o LED para indicar desconexão
        Hardware::setLedState(Hardware::LED_OFF);
    }
}
