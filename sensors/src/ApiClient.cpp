/**
 * @file ApiClient.cpp
 * @brief Implementação da classe ApiClient.
 */

#include "ApiClient.h"
#include "Config.h"
#include "LogSystem.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h> // Usaremos para criar o corpo da requisição

#define MODULE_NAME "ApiClient"

ApiClient::ApiClient(const char* endpointUrl) : m_endpointUrl(endpointUrl) {
    LOG_INFO(MODULE_NAME, "Cliente de API inicializado. Endpoint: %s", m_endpointUrl.c_str());
}

bool ApiClient::sendData(const SensorData& data) {
    // 1. Verifica se estamos conectados ao WiFi
    if (WiFi.status() != WL_CONNECTED) {
        LOG_WARN(MODULE_NAME, "Não conectado ao WiFi. Envio cancelado.");
        return false;
    }

    // 2. Cria o corpo da requisição (payload) em formato JSON
    StaticJsonDocument<256> doc;
    doc["temperatura"] = data.temperature;
    doc["umidade"] = data.humidityPercent;
    doc["timestamp"] = data.timestamp;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    // 3. Prepara e envia a requisição HTTP POST
    HTTPClient http;
    bool success = false;

    // Inicia a requisição
    if (http.begin(m_endpointUrl)) {
        LOG_INFO(MODULE_NAME, "Enviando dados para a API...");
        
        // Define o cabeçalho da requisição
        http.addHeader("Content-Type", "application/json");

        // Envia o POST com o payload JSON
        int httpCode = http.POST(jsonPayload);

        // 4. Analisa a resposta
        if (httpCode > 0) {
            LOG_INFO(MODULE_NAME, "Resposta da API: %d", httpCode);
            String responseBody = http.getString();
            LOG_DEBUG(MODULE_NAME, "Corpo da resposta: %s", responseBody.c_str());

            // Consideramos sucesso se o código for da família 2xx (ex: 200 OK, 201 Created)
            if (httpCode >= 200 && httpCode < 300) {
                success = true;
                LOG_INFO(MODULE_NAME, "Dados enviados com sucesso!");
            } else {
                LOG_ERROR(MODULE_NAME, "Falha no envio, código de erro HTTP: %d", httpCode);
            }
        } else {
            LOG_ERROR(MODULE_NAME, "Falha na conexão com a API. Erro: %s", http.errorToString(httpCode).c_str());
        }

        // Libera os recursos
        http.end();
    } else {
        LOG_ERROR(MODULE_NAME, "Não foi possível iniciar a conexão HTTP.");
    }

    return success;
}
