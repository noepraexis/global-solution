/**
 * @file DataModel.cpp
 * @brief Implementação das funções do modelo de dados.
 *
 * Este arquivo implementa qualquer funcionalidade adicional relacionada ao
 * modelo de dados que não esteja definida inline no arquivo DataTypes.h.
 */

#include "DataTypes.h"
#include "Config.h"
#include <Arduino.h>

// Funções auxiliares para processamento de dados de sensores

float convertRawToHumidityPercent(uint16_t rawValue) {
    // Conversão otimizada de valor bruto para percentual de umidade (0-100%)
    return (static_cast<float>(rawValue) * 100.0f) / 4095.0f;
}

// Implementações adicionais poderiam ser adicionadas aqui conforme necessário.
// Por exemplo, funções para calibração de sensores, filtros adicionais, etc.