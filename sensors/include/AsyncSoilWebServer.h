/**
 * @file AsyncSoilWebServer.h
 * @brief Servidor web assíncrono com WebSockets.
 */

#ifndef ASYNC_SOIL_WEB_SERVER_H
#define ASYNC_SOIL_WEB_SERVER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "DataTypes.h"
#include "SensorManager.h"
#include "SystemMonitor.h"
#include "WiFiManager.h"
#include "MemoryManager.h"

/**
 * Classe para servidor web assíncrono com WebSockets
 *
 * Implementa interface web de baixa latência com atualizações em tempo
 * real usando WebSockets e servidor assíncrono.
 */
class AsyncSoilWebServer {
private:
    AsyncWebServer m_server;           // Servidor web assíncrono
    AsyncWebSocket m_websocket;        // Servidor WebSocket
    SensorManager &m_sensorManager;    // Referência para o gerenciador de sensores

    uint32_t m_lastBroadcastTime;      // Timestamp da última broadcast
    uint16_t m_clientCount;            // Contador de clientes WebSocket
    uint32_t m_broadcastCount;         // Contador de broadcasts

    // HTML da página principal (armazenado em PROGMEM para economizar RAM)
    static const char INDEX_HTML[] PROGMEM;

    /**
     * Manipulador de eventos WebSocket.
     *
     * @param server Ponteiro para o servidor WebSocket.
     * @param client Ponteiro para o cliente WebSocket.
     * @param type Tipo de evento.
     * @param data Dados do evento.
     * @param len Comprimento dos dados.
     */
    void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                            AwsEventType type, void *arg, uint8_t *data, size_t len);

    /**
     * Processa comandos recebidos via WebSocket.
     *
     * @param client Ponteiro para o cliente WebSocket.
     * @param data Dados do comando.
     * @param len Comprimento dos dados.
     */
    void processWebSocketCommand(AsyncWebSocketClient *client, uint8_t *data, size_t len);

    /**
     * Callback para eventos WebSocket.
     *
     * @param server Ponteiro para o servidor WebSocket.
     * @param client Ponteiro para o cliente WebSocket.
     * @param type Tipo de evento.
     * @param arg Argumentos.
     * @param data Dados do evento.
     * @param len Comprimento dos dados.
     */
    static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                            AwsEventType type, void *arg, uint8_t *data, size_t len);

    /**
     * Handler para a rota principal.
     *
     * @param request Ponteiro para a requisição HTTP.
     */
    void handleRoot(AsyncWebServerRequest *request);

    /**
     * Handler para a rota de dados.
     *
     * @param request Ponteiro para a requisição HTTP.
     */
    void handleData(AsyncWebServerRequest *request);

    /**
     * Handler para a rota de logs do sistema.
     *
     * @param request Ponteiro para a requisição HTTP.
     */
    void handleLogs(AsyncWebServerRequest *request);

    /**
     * Handler para requisições não encontradas.
     *
     * @param request Ponteiro para a requisição HTTP.
     */
    void handleNotFound(AsyncWebServerRequest *request);

    /**
     * Prepara mensagem JSON com dados dos sensores.
     *
     * @param dataOnly Se true, inclui apenas dados dos sensores.
     *                 Se false, inclui também estatísticas do sistema.
     * @return Mensagem JSON formatada.
     */
    String prepareJsonMessage(bool dataOnly = false);

public:
    /**
     * Construtor do servidor web.
     *
     * @param port Porta para o servidor web.
     * @param sensorManager Referência para o gerenciador de sensores.
     */
    AsyncSoilWebServer(uint16_t port, SensorManager &sensorManager);

    /**
     * Inicializa o servidor web.
     *
     * @return true se a inicialização foi bem-sucedida.
     */
    bool begin();

    /**
     * Atualiza o servidor web.
     *
     * @param forceUpdate Force o envio de atualizações, mesmo sem intervalo.
     * @return true se enviou atualizações.
     */
    bool update(bool forceUpdate = false);

    /**
     * Obtém o número de clientes conectados.
     *
     * @return Número de clientes WebSocket.
     */
    uint16_t getClientCount() const;

    /**
     * Envia mensagem para todos os clientes WebSocket.
     *
     * @param message Mensagem a ser enviada.
     * @return true se a mensagem foi enviada para pelo menos um cliente.
     */
    bool broadcastMessage(const String &message);

    /**
     * Limpa clientes inativos.
     *
     * @return Número de clientes removidos.
     */
    uint16_t cleanClients();
};

#endif // ASYNC_SOIL_WEB_SERVER_H
