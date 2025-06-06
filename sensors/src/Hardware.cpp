/**
 * @file Hardware.cpp
 * @brief Implementa as funções de hardware do sistema.
 */

#include "Hardware.h"
#include "LogSystem.h"

// Nome do módulo para logs
#define MODULE_NAME "Hardware"

namespace Hardware {

    // Instância do sensor DHT
    DHT dht(PIN_DHT22_SENSOR, DHT_TYPE);

    // Estrutura para armazenar os valores mais recentes dos sensores
    struct SensorValues {
        float temperature;    // Temperatura bruta
        float correctedTemp;  // Temperatura corrigida
        float humidity;       // Umidade
        bool needsUpdate;     // Flag para atualização do display
    };

    // Valores atuais dos sensores (globais ao namespace)
    SensorValues g_currentValues = {25.0f, 25.0f, 50.0f, false};

    void setupPins() {
        LOG_INFO(MODULE_NAME, "Configurando hardware");

        // Configura o pino do LED como saída
        pinMode(PIN_LED_INDICATOR, OUTPUT);

        // Inicializa o LED como desligado
        digitalWrite(PIN_LED_INDICATOR, LED_OFF);

        // Os pinos analógicos não precisam de configuração no ESP32

        // Inicializa o sensor DHT22
        if (initDHT()) {
            LOG_INFO(MODULE_NAME, "Sensor DHT22 inicializado com sucesso (%.1f°C)", g_currentValues.temperature);
        } else {
            LOG_ERROR(MODULE_NAME, "Falha ao inicializar o sensor DHT22");
        }

        LOG_INFO(MODULE_NAME, "Pinos configurados e dispositivos inicializados");
    }

    void IRAM_ATTR setLedState(LedState state) {
        // Função colocada na IRAM para execução mais rápida
        digitalWrite(PIN_LED_INDICATOR, state);
    }

    void IRAM_ATTR toggleLed() {
        // Alterna o estado do LED (otimizado para IRAM)
        static bool ledState = false;
        ledState = !ledState;
        digitalWrite(PIN_LED_INDICATOR, ledState ? LED_ON : LED_OFF);
    }

    uint16_t readAnalogAverage(uint8_t pin, uint8_t samples) {
        // Limita o número de amostras para evitar overflow
        if (samples == 0) samples = 1;
        if (samples > 64) samples = 64;

        // Realiza múltiplas leituras e calcula a média
        uint32_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += analogRead(pin);
            // Pequeno delay para estabilizar o ADC
            delayMicroseconds(100);
        }

        // Retorna a média
        return static_cast<uint16_t>(sum / samples);
    }

    bool readButtonDebounced(uint8_t pin, int activeState) {
        // Implementação estável de debounce
        static uint32_t lastDebounceTime[40] = {0};
        static int lastButtonState[40] = {HIGH}; // Inicializa com HIGH (estado normal do INPUT_PULLUP)
        static int stableButtonState[40] = {HIGH}; // Estado confirmado após debounce

        int pinIndex = pin % 40;
        int reading = digitalRead(pin);

        // Se a leitura mudou
        if (reading != lastButtonState[pinIndex]) {
            // Reset do timer de debounce
            lastDebounceTime[pinIndex] = millis();
        }

        lastButtonState[pinIndex] = reading;

        // Se o tempo de debounce passou (50ms)
        if ((millis() - lastDebounceTime[pinIndex]) > 50) {
            // Se o estado atual é diferente do estado estável
            if (reading != stableButtonState[pinIndex]) {
                stableButtonState[pinIndex] = reading;
            }
        }

        // Retorna true se o botão estiver no estado ativo
        return (stableButtonState[pinIndex] == activeState);
    }

    bool initDHT() {
        // Configura o pino para garantir que esteja no modo correto
        pinMode(PIN_DHT22_SENSOR, INPUT_PULLUP);
        delay(10); // Pequeno delay para estabilização

        // Inicializa o sensor DHT com tempo de pull-up maior para maior estabilidade
        dht.begin(60); // 60μs de tempo de pull-up (valor padrão é 55)

        // Aguarda um tempo maior para o sensor estabilizar após inicialização
        delay(1000);

        // Tenta realizar uma leitura de teste
        float testReading = dht.readTemperature();

        // Verifica se a leitura foi bem-sucedida
        if (isnan(testReading)) {
            if (DEBUG_MODE) {
                LOG_DEBUG(MODULE_NAME, "Falha ao inicializar o sensor DHT22");
            }

            // Segunda tentativa após um delay maior
            delay(2000);
            testReading = dht.readTemperature();

            if (isnan(testReading)) {
                if (DEBUG_MODE) {
                    LOG_DEBUG(MODULE_NAME, "Falha persistente no sensor DHT22");
                }
                return false;
            }
        }

        // Armazena a primeira leitura válida
        g_currentValues.temperature = testReading;
        g_currentValues.correctedTemp = testReading;
        g_currentValues.needsUpdate = true;

        return true;
    }

    float readTemperature() {
        // Lê a temperatura do sensor DHT22 em graus Celsius com tentativa de recuperação
        static float lastValidTemperature = 25.0f; // Valor padrão razoável

        // Os parâmetros são:
        // 1º: S (Scale) - false para Celsius, true para Fahrenheit
        // 2º: force - true para forçar nova leitura (ignorando o cache da biblioteca)
        float temperature = dht.readTemperature(false, true);

        if (isnan(temperature)) {
            if (DEBUG_MODE) {
                LOG_DEBUG(MODULE_NAME, "Erro na leitura de temperatura do DHT22, tentando novamente");
            }

            // Tenta novamente após um curto delay
            delay(500);
            temperature = dht.readTemperature(false, true);

            if (isnan(temperature)) {
                if (DEBUG_MODE) {
                    LOG_DEBUG(MODULE_NAME, "Falha persistente, usando último valor válido");
                }
                // Usa último valor válido, mas não marca como needsUpdate
                g_currentValues.needsUpdate = false;
                return getCalibrationTemperature(lastValidTemperature);
            }
        }

        // Verificação de valores absurdos (fora da faixa esperada)
        if (temperature < -40.0f || temperature > 80.0f) {
            if (DEBUG_MODE) {
                LOG_DEBUG(MODULE_NAME, "Temperatura fora da faixa válida: %.1f°C, usando último valor", temperature);
            }
            g_currentValues.needsUpdate = false;
            return getCalibrationTemperature(lastValidTemperature);
        }

        // Atualiza o último valor válido
        lastValidTemperature = temperature;

        // Atualiza os valores atuais
        g_currentValues.temperature = temperature;
        g_currentValues.needsUpdate = true;

        // Exibe a leitura apenas se estiver no modo de debug e numa periodicidade específica
        // Aumentamos para 10s para evitar poluir demais o console, já que exibimos
        // informações contínuas com updateLine
        static uint32_t lastTempDebugTime = 0;
        if (DEBUG_MODE && (millis() - lastTempDebugTime > 10000)) {
            LOG_INFO(MODULE_NAME, "Resumo Periódico de Sensores");
            LOG_INFO(MODULE_NAME, "Temperatura: %.1f°C    Umidade: %.1f%%",
                g_currentValues.correctedTemp, g_currentValues.humidity);
            LOG_INFO(MODULE_NAME, "Estado: Atualizado recentemente");
            lastTempDebugTime = millis();
        }

        // Retorna a temperatura calibrada para sincronização com interface web
        return getCalibrationTemperature(temperature);
    }

    float getCalibrationTemperature(float rawTemp) {
        // Aplica uma correção de calibração para compensar erros
        // sistemáticos do sensor com base em medições de referência

        // Aplica a correção para temperatura mais precisa (valor direto do sensor)
        // Para usar fatores de calibração, descomente e ajuste as linhas abaixo:
        // float correctedTemp = rawTemp * 1.0f + 0.0f;  // Ajuste com fator e offset
        float correctedTemp = rawTemp;  // Por enquanto, sem correção aplicada

        // Limita a faixa de temperatura para evitar valores absurdos
        correctedTemp = constrain(correctedTemp, -40.0f, 80.0f);

        // Atualiza o valor armazenado
        g_currentValues.correctedTemp = correctedTemp;

        return correctedTemp;
    }

    float readHumidity() {
        // Lê a umidade relativa do sensor DHT22 (0-100%) com tentativa de recuperação
        static float lastValidHumidity = 50.0f;  // Valor padrão razoável
        float humidity = dht.readHumidity(true); // Force = true para forçar nova leitura

        if (isnan(humidity)) {
            if (DEBUG_MODE) {
                LOG_DEBUG(MODULE_NAME, "Erro na leitura de umidade do DHT22, tentando novamente");
            }

            // Tenta novamente após um curto delay
            delay(500);
            humidity = dht.readHumidity(true);

            if (isnan(humidity)) {
                if (DEBUG_MODE) {
                    LOG_DEBUG(MODULE_NAME, "Falha persistente, usando último valor válido");
                }
                g_currentValues.needsUpdate = false;
                return lastValidHumidity; // Retorna último valor válido
            }
        }

        // Filtra valores absurdos
        if (humidity < 0.0f || humidity > 100.0f) {
            if (DEBUG_MODE) {
                LOG_DEBUG(MODULE_NAME, "Valor de umidade fora da faixa (%.1f%%), usando último valor válido", humidity);
            }
            g_currentValues.needsUpdate = false;
            return lastValidHumidity;
        }

        // Atualiza o último valor válido
        lastValidHumidity = humidity;

        // Atualiza valores atuais
        g_currentValues.humidity = humidity;
        g_currentValues.needsUpdate = true;

        return humidity;
    }
} // namespace Hardware
