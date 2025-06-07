/**
 * @file WifiPerformance.h
 * @brief Define a interface para o módulo de inicialização otimizada do WiFi.
 */

#ifndef WIFI_PERFORMANCE_H
#define WIFI_PERFORMANCE_H

#include <Arduino.h>
// Definitions for forward declaration
typedef struct QueueDefinition* SemaphoreHandle_t;

// Interface para comunicação com o inicializador antecipado
extern "C" {
    // Indica se a inicialização antecipada foi concluída
    extern volatile bool g_wifiEarlyInitDone;

    // Indica se a inicialização antecipada teve sucesso
    extern volatile bool g_wifiEarlyInitSuccess;
}

// Classe para coordenação da inicialização de WiFi
class WifiPerformanceInitializer {
public:
    // Construtor - não inicializa WiFi
    WifiPerformanceInitializer();

    /**
     * Inicializa o WiFi em momento apropriado.
     *
     * Substitui a inicialização via constructor attribute para evitar
     * problemas de boot. Deve ser chamado explicitamente após
     * a inicialização básica do sistema.
     *
     * @return true se iniciou o processo de inicialização.
     */
    bool begin();

    /**
     * Sinaliza o semáforo de WiFi se a inicialização teve sucesso.
     *
     * @param semaphore Semáforo a ser sinalizado.
     */
    static void signalWiFiSemaphore(SemaphoreHandle_t semaphore);

    /**
     * Obtém a instância do inicializador.
     *
     * @return Referência para a instância estática.
     */
    static WifiPerformanceInitializer& getInstance();

    /**
     * Handler de eventos WiFi encapsulado na classe.
     *
     * @param event Tipo de evento WiFi.
     * @param info Informações adicionais do evento.
     */
    static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

private:
    // Instância estática para padrão singleton
    static WifiPerformanceInitializer* s_instance;
};

#endif // WIFI_PERFORMANCE_H