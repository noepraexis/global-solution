/**
 * @file WiFiManager.h
 * @brief Gerencia a conexão WiFi consistentemente.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"
#include "Hardware.h"

/**
 * Classe para gerenciar a conexão WiFi consistentemente.
 *
 * Implementa estratégias de reconexão automática e monitoramento
 * de qualidade de sinal.
 */
class WiFiManager {
private:
    // Singleton
    static WiFiManager *s_instance;

    // Estado atual da conexão
    bool m_connected;

    // Força do sinal (RSSI)
    int16_t m_rssi;

    // Timestamp para reconexão
    uint32_t m_reconnectTime;

    // Contador de tentativas de reconexão
    uint8_t m_reconnectAttempts;

    // Endereço IP
    IPAddress m_ipAddress;

    // Handler de eventos WiFi
    static void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

    // Prepara e envia telemetria de status WiFi
    void prepareTelemetry();

    // Construtor privado (singleton)
    WiFiManager();

public:
    /**
     * Obtém a instância do singleton.
     *
     * @return Referência para a instância única.
     */
    static WiFiManager &getInstance();

    /**
     * Inicializa a conexão WiFi.
     *
     * @param ssid Nome da rede WiFi.
     * @param password Senha da rede WiFi.
     * @return true se iniciou o processo de conexão.
     */
    bool connect(const char *ssid, const char *password);

    /**
     * Atualiza o estado da conexão WiFi.
     *
     * @return true se conectado, false caso contrário.
     */
    bool update();

    /**
     * Verifica se o WiFi está conectado.
     *
     * @return true se conectado, false caso contrário.
     */
    bool isConnected() const;

    /**
     * Obtém o endereço IP.
     *
     * @return Objeto IPAddress com o endereço IP.
     */
    IPAddress getIP() const;

    /**
     * Obtém a força do sinal WiFi.
     *
     * @return Valor RSSI (geralmente entre -100 e 0).
     */
    int16_t getRSSI();

    /**
     * Obtém o status da conexão em formato texto.
     *
     * @param buffer Buffer para armazenar a string.
     * @param size Tamanho do buffer.
     * @return Ponteiro para o buffer.
     */
    char* getStatusString(char* buffer, size_t size);

    /**
     * Desconecta o WiFi.
     */
    void disconnect();
};

#endif // WIFI_MANAGER_H
