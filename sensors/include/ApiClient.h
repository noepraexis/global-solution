/**
 * @file ApiClient.h
 * @brief Define a classe para enviar dados para um endpoint de API externo.
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include "DataTypes.h" // Usaremos a struct SensorData

class ApiClient {
public:
    /**
     * @brief Construtor.
     * @param endpointUrl A URL completa do endpoint da API.
     */
    ApiClient(const char* endpointUrl);

    /**
     * @brief Envia os dados dos sensores para a API.
     * @param data Os dados dos sensores a serem enviados.
     * @return true se o envio foi bem-sucedido (código HTTP 2xx), false caso contrário.
     */
    bool sendData(const SensorData& data);

private:
    String m_endpointUrl; // Armazena a URL da API
};

#endif // API_CLIENT_H