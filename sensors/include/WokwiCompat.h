/**
 * @file WokwiCompat.h
 * @brief Define funções de compatibilidade para o ambiente de simulação Wokwi.
 */

#ifndef WOKWI_COMPAT_H
#define WOKWI_COMPAT_H

#include <Arduino.h>
#include <WiFi.h>
#include "ConsoleFormat.h"

// Define para ambiente Wokwi (será usado para identificação em tempo de compilação)
#define WOKWI_ENV 1

namespace WokwiCompat {

    /**
     * Inicializa configurações de compatibilidade específicas para Wokwi.
     *
     * Ajusta parâmetros críticos para evitar erros de asserção no simulador:
     * - Frequência de CPU reduzida
     * - Configurações de WiFi otimizadas
     * - Delays específicos para sincronização
     */
    void init() {
        CONSOLE_BEGIN_SECTION("Compatibilidade Wokwi");
        CONSOLE_PRINTLN("Iniciando compatibilidade para ambiente Wokwi...");

        // Reduz frequência da CPU para melhor estabilidade no simulador
        setCpuFrequencyMhz(80);

        // Desativa o modo de economia de energia do WiFi para maior estabilidade
        WiFi.setSleep(false);

        // Aplica pequeno delay para estabilização
        delay(200);

        CONSOLE_PRINTLN("Configurada - CPU @ 80MHz, WiFi otimizado");
        CONSOLE_END_SECTION();

        // Pequeno delay adicional para garantir sequência de mensagens
        delay(100);
    }

    /**
     * Realiza conexão WiFi otimizada para ambiente Wokwi.
     *
     * Usa configurações específicas que evitam problemas de asserção no simulador:
     * - Canal 6 específico para "Wokwi-GUEST"
     * - Timeout adaptado para o simulador
     *
     * @param ssid Nome da rede WiFi.
     * @param password Senha da rede WiFi.
     * @param timeout_ms Tempo limite para conexão em ms.
     * @return true se a conexão foi iniciada com sucesso.
     */
    bool connectWiFi(const char *ssid, const char *password, uint32_t timeout_ms = 10000) {
        // Utilizamos o sistema de console formatado para mensagens consistentes
        CONSOLE_BEGIN_SECTION("Conexão WiFi Wokwi");
        CONSOLE_PRINTLN("Conectando ao WiFi %s no canal 6...", ssid);

        // Configurações otimizadas para Wokwi
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);

        // Inicia conexão especificando o canal 6 (crucial para o Wokwi)
        WiFi.begin(ssid, password, 6);

        // Aguarda conexão com timeout
        uint32_t start = millis();
        uint32_t dots = 0;

        // Indicador de progresso com mensagem fixa, sem poluir o console
        CONSOLE_PRINTLN("Aguardando conexão");

        while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
            // A cada 500ms atualiza o indicador
            if (dots % 5 == 0) {
                CONSOLE_UPDATE_LINE("Conectando %.*s", (dots % 10) + 1, "..........");
            }
            dots++;
            delay(100);
        }

        // Garante um delay antes da mensagem de resultado
        delay(100);

        if (WiFi.status() == WL_CONNECTED) {
            // Mensagem de sucesso com formatação adequada
            CONSOLE_PRINTLN("WiFi conectado com sucesso");
            CONSOLE_PRINTLN("IP atribuído: %s", WiFi.localIP().toString().c_str());
            CONSOLE_PRINTLN("Canal: %d  RSSI: %d dBm", WiFi.channel(), WiFi.RSSI());
            CONSOLE_END_SECTION();

            // Delay adicional para garantir que as mensagens não sejam interrompidas
            delay(100);
            return true;
        } else {
            CONSOLE_PRINTLN("Falha na conexão WiFi após %u ms", timeout_ms);
            CONSOLE_END_SECTION();
            return false;
        }
    }

} // namespace WokwiCompat

#endif // WOKWI_COMPAT_H