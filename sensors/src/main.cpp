#include <Arduino.h>

// Apenas um teste para explorar o funcionamento do FreeRTOS

void readSensorTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);

    while (true) {
        Serial.printf("[%lu ms] Lendo sensor...\n", millis());
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

void sendDataTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(2000);

    while (true) {
        Serial.printf("[%lu ms] Enviando dados...\n", millis());
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

void setup() {
    Serial.begin(115200);

    
    xTaskCreatePinnedToCore(
        readSensorTask, "ReadSensor", 10000, NULL, 1, NULL, 1
    );

    xTaskCreatePinnedToCore(
        sendDataTask, "SendData", 10000, NULL, 1, NULL, 0
    );

}

void loop() {
    // O loop principal está vazio pois usamos FreeRTOS para tarefas

    // O monitoramento do sistema e sensores agora é feito
    // diretamente pelo SensorManager com seu método de
    // atualização na mesma linha que cicla entre diferentes
    // visualizações de dados
}
