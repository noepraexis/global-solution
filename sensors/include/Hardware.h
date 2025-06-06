/**
 * @file Hardware.h
 * @brief Define as constantes e funções relacionadas ao hardware.
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include "Config.h"

namespace Hardware {
    // Pinos dos sensores
    constexpr uint8_t PIN_DHT22_SENSOR = 23;    // Sensor digital DHT22 para temperatura e umidade (pino 23 é bidirecional)
    constexpr uint8_t PIN_LED_INDICATOR = 26;   // LED indicador

    // Tipo do sensor DHT
    constexpr uint8_t DHT_TYPE = DHT22;         // Usar DHT22 em vez de DHT11

    // Estados de LED
    enum LedState {
        LED_OFF = LOW,
        LED_ON  = HIGH
    };

    // Estados do relé de irrigação
    enum RelayState {
        RELAY_OFF = LOW,   // Estado seguro (fail-safe)
        RELAY_ON  = HIGH   // Relé ativado
    };

    /**
     * Configura todos os pinos necessários para o sistema.
     */
    void setupPins();

    /**
     * Define o estado do LED indicador.
     *
     * @param state Estado desejado para o LED (HIGH ou LOW).
     */
    void IRAM_ATTR setLedState(LedState state);

    /**
     * Alterna o estado do LED indicador.
     *
     * Função otimizada para execução rápida.
     */
    void IRAM_ATTR toggleLed();

    /**
     * Lê valor analógico com múltiplas amostras para reduzir ruído.
     *
     * @param pin Pino a ser lido.
     * @param samples Número de amostras para média.
     * @return Média das leituras.
     */
    uint16_t readAnalogAverage(uint8_t pin, uint8_t samples = 5);

    /**
     * Lê o botão com debounce por software.
     *
     * @param pin Pino do botão.
     * @param activeState Estado ativo do botão (HIGH ou LOW).
     * @return true se o botão está pressionado, false caso contrário.
     */
    bool readButtonDebounced(uint8_t pin, int activeState = LOW);

    /**
     * Inicializa o sensor DHT22.
     *
     * @return true se a inicialização foi bem-sucedida.
     */
    bool initDHT();

    /**
     * Lê a temperatura do sensor DHT22.
     *
     * @return Temperatura em graus Celsius calibrada ou NAN se ocorrer erro.
     */
    float readTemperature();

    /**
     * Lê a umidade do sensor DHT22.
     *
     * @return Umidade relativa (0-100%) ou NAN se ocorrer erro.
     */
    float readHumidity();

    /**
     * Obtém a temperatura calibrada para exibição na web.
     *
     * @param rawTemp Temperatura bruta do sensor
     * @return Temperatura calibrada para exibição
     */
    float getCalibrationTemperature(float rawTemp);

    /**
     * Define o estado do relé de irrigação.
     *
     * @param state Estado desejado para o relé (LOW ou HIGH).
     */
    void IRAM_ATTR setRelayState(RelayState state);

    /**
     * Alterna o estado do relé de irrigação.
     *
     * Função otimizada para execução rápida.
     */
    void IRAM_ATTR toggleRelay();

    /**
     * Obtém o estado atual do relé de irrigação.
     *
     * @return true se o relé está ativado, false caso contrário.
     */
    bool getRelayState();

} // namespace Hardware

#endif // HARDWARE_H
