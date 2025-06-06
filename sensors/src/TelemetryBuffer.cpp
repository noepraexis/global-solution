/**
 * @file TelemetryBuffer.cpp
 * @brief Implementação do buffer estruturado para dados de telemetria.
 */

#include "TelemetryBuffer.h"

TelemetryBuffer::TelemetryBuffer()
    : temperature(0.0f),
      humidity(0.0f),
      freeHeap(0),
      heapFragmentation(0),
      uptime(0),
      wifiRssi(0),
      timestamp(0),
      readCount(0) {
    memset(ipAddress, 0, sizeof(ipAddress));
}

void TelemetryBuffer::toJson(JsonObject& json) const {
    // Criar objetos filhos para cada categoria
    JsonObject sensors = json.createNestedObject("sensors");

    // Adicionar dados de sensores
    sensors["temperature"] = temperature;
    sensors["humidity"] = humidity;
    sensors["timestamp"] = timestamp;
    sensors["readCount"] = readCount;

    // Adicionar estatísticas do sistema
    JsonObject stats = json.createNestedObject("stats");
    stats["freeHeap"] = freeHeap;
    stats["fragmentation"] = heapFragmentation;
    stats["uptime"] = uptime;
    stats["wifiRssi"] = wifiRssi;
    stats["ipAddress"] = ipAddress;
}

char* TelemetryBuffer::toConsoleString(char* buffer, size_t bufferSize, TelemetryType type) const {
    switch (type) {
        case TelemetryType::SENSORS:
            snprintf(buffer, bufferSize,
                "Sensores → Temp: %.1f °C  Umid: %.1f%%",
                temperature, humidity);
            break;

        case TelemetryType::SYSTEM:
            snprintf(buffer, bufferSize,
                "Sistema  → Tempo: %-5u s  Heap: %-7u bytes  Frag: %u%%",
                uptime, freeHeap, heapFragmentation);
            break;

        case TelemetryType::WIFI:
            {
                // Determina a qualidade do sinal
                const char* signalQuality;
                if (wifiRssi < -80) signalQuality = "Ruim";
                else if (wifiRssi < -70) signalQuality = "Regular";
                else if (wifiRssi < -60) signalQuality = "Bom";
                else signalQuality = "Excelente";

                snprintf(buffer, bufferSize,
                    "WiFi Status → IP: %s | RSSI: %d dBm | Sinal: %s",
                    ipAddress, wifiRssi, signalQuality);
            }
            break;

        case TelemetryType::ALL:
            // Implementação que combina todos os tipos
            // Limitado pelo tamanho do buffer, então pode ser mais resumido
            snprintf(buffer, bufferSize,
                "Sensores: T=%.1f°C H=%.1f%% | Sys: Heap=%u Up=%us | WiFi: %s",
                temperature, humidity,
                freeHeap, uptime,
                ipAddress);
            break;
    }

    return buffer;
}