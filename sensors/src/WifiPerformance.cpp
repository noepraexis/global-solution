// src/WifiPerformance.cpp
/**
 * @file WifiPerformance.cpp
 * @brief Implementa inicialização otimizada do WiFi para ambientes de performance.
 */

#include "WiFiManager.h"
#include "Config.h"
#include "WifiPerformance.h"
#include "LogSystem.h"
// Incluindo cabeçalhos do FreeRTOS diretamente no arquivo de implementação
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Nome do módulo para logs
#define MODULE_NAME "WifiPerf"

// Variável global para comunicação entre inicializador antecipado e Main.cpp
extern "C" {
    volatile bool g_wifiEarlyInitDone = false;
    volatile bool g_wifiEarlyInitSuccess = false;
}

// Handler de eventos do WiFi agora implementado como método estático da classe
// para melhor encapsulamento e evitar conflitos de namespace
void WifiPerformanceInitializer::wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    // Limita-se apenas ao evento mais crítico: conexão bem-sucedida
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        // Evita operações complexas, apenas registra status básico
        LOG_INFO(MODULE_NAME, "Módulo WiFi inicializado");
        LOG_INFO(MODULE_NAME, "Status: %d (WL_CONNECTED=%d)", WiFi.status(), WL_CONNECTED);
        LOG_INFO(MODULE_NAME, "Resultado: Conectado");
        {
            IPAddress ip = WiFi.localIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            LOG_INFO(MODULE_NAME, "IP: %s", ipStr);
        }

        // Marca inicialização como bem-sucedida
        g_wifiEarlyInitSuccess = true;
    }
    // Para outros eventos, apenas registra uma mensagem simples
    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        LOG_WARN(MODULE_NAME, "WiFi desconectado durante inicialização antecipada");
    }
}

// Inicialização da variável estática singleton
WifiPerformanceInitializer* WifiPerformanceInitializer::s_instance = nullptr;

// Implementação do construtor da classe declarada em WifiPerformance.h
WifiPerformanceInitializer::WifiPerformanceInitializer() {
    // Implementação mínima do construtor que não faz inicialização de WiFi
    // Agora apenas marca como não inicializado
    g_wifiEarlyInitDone = false;
    g_wifiEarlyInitSuccess = false;
}

// Método para obter a instância do singleton
WifiPerformanceInitializer& WifiPerformanceInitializer::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new WifiPerformanceInitializer();
    }
    return *s_instance;
}

// Método de inicialização explícita que substitui o antigo constructor
bool WifiPerformanceInitializer::begin() {
    LOG_INFO(MODULE_NAME, "Inicializando módulo de performance WiFi");

    // Marca a inicialização como em andamento antes de qualquer outra operação
    g_wifiEarlyInitDone = true;
    g_wifiEarlyInitSuccess = false;

    // Configuração básica, evita operações potencialmente instáveis
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    // Registra o handler de eventos APÓS a configuração básica
    // e antes de iniciar a conexão
    WiFi.onEvent(WifiPerformanceInitializer::wifiEventHandler);
    LOG_INFO(MODULE_NAME, "Handler de eventos WiFi registrado");

    // Inicia a conexão WiFi de forma simples e não-bloqueante
    #if defined(WOKWI_ENV) || defined(WOKWI)
        // Especial para Wokwi - Canal 6 é OBRIGATÓRIO para funcionamento
        LOG_INFO(MODULE_NAME, "Iniciando WiFi para ambiente Wokwi (canal 6 obrigatório)");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
    #else
        LOG_INFO(MODULE_NAME, "Iniciando WiFi com configuração padrão");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    #endif

    LOG_INFO(MODULE_NAME, "Inicialização WiFi delegada ao sistema de eventos");
    LOG_INFO(MODULE_NAME, "Continuando inicialização do sistema sem bloqueio");

    return true;
}

// Implementação da função estática fora da definição da classe
// para evitar problemas de linkagem
void WifiPerformanceInitializer::signalWiFiSemaphore(SemaphoreHandle_t semaphore) {
    // Verifica se o semáforo é válido
    if (semaphore == nullptr) {
        LOG_WARN(MODULE_NAME, "Semáforo WiFi não disponível");
        return;
    }

    // Verifica se a inicialização do WiFi teve sucesso
    if (g_wifiEarlyInitSuccess) {
        LOG_INFO(MODULE_NAME, "Sinalizando semáforo de WiFi pelo módulo de performance");
        xSemaphoreGive(semaphore);
        return;
    }

    // Se o WiFi não está inicializado ou teve falha
    if (g_wifiEarlyInitDone) {
        LOG_WARN(MODULE_NAME, "Inicialização antecipada do WiFi falhou, não sinalizando semáforo");
    } else {
        LOG_WARN(MODULE_NAME, "Inicialização do WiFi ainda não concluída, não sinalizando semáforo");
    }
}

// Instância singleton será criada sob demanda pelo método getInstance()
// Não precisamos mais da instância estática aqui