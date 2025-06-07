// src/main.cpp
/**
 * @file main.cpp
 * @brief Arquivo principal do Sistema de Monitoramento do Solo.
 */

#include <Arduino.h>
#include "Config.h"
#include "Hardware.h"
#include "SensorManager.h"
#include "AsyncSoilWebServer.h"
#include "WiFiManager.h"
#include "SystemMonitor.h"
#include "MemoryManager.h"
#include "WokwiCompat.h"
#include "WifiPerformance.h"
#include "LogSystem.h"
#include "OutputManager.h"

// Define o nome do módulo para logging
#define MODULE_NAME "Main"

// Instâncias dos componentes
SensorManager *g_sensorManager = nullptr;
AsyncSoilWebServer *g_webServer = nullptr;

// Tarefas FreeRTOS
TaskHandle_t g_sensorTask = nullptr;
TaskHandle_t g_webTask = nullptr;

// Semáforos para sincronização
SemaphoreHandle_t g_sensorMutex = nullptr;

// Semáforo para sincronização de WiFi
SemaphoreHandle_t g_wifiConnectedSemaphore = nullptr;

/**
 * Handler de eventos WiFi para sinalização de conexão.
 *
 * Esta versão melhorada evita conflitos com outros handlers
 * e implementa um mecanismo mais rigoroso de sinalização.
 */
void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    static bool semaphoreDone = false;

    // Previne sinalização múltipla do semáforo
    if (semaphoreDone) {
        return;
    }

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            LOG_INFO(MODULE_NAME, "Evento WiFi: GOT_IP recebido");
            {
                IPAddress ip = WiFi.localIP();
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                LOG_INFO(MODULE_NAME, "IP atribuído: %s", ipStr);
            }

            if (g_wifiConnectedSemaphore != nullptr) {
                // Pequeno delay para garantir sincronização de mensagens no console
                delay(100);

                // Marca que o semáforo já foi dado para evitar duplicação
                semaphoreDone = true;

                // Libera o semáforo para continuar a inicialização
                xSemaphoreGive(g_wifiConnectedSemaphore);

                LOG_INFO(MODULE_NAME, "Semáforo WiFi liberado pelo handler de Main");
            } else {
                LOG_WARN(MODULE_NAME, "AVISO: Semáforo WiFi não disponível!");
            }
                break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // Se desconectar, reinicia o flag para permitir nova sinalização
            // Se reconectar, o semáforo será sinalizado novamente
            semaphoreDone = false;
            LOG_WARN(MODULE_NAME, "WiFi desconectado - aguardando reconexão");
            break;

        default:
            // Ignora outros eventos
            break;
    }
}

/**
 * Tarefa responsável pela leitura dos sensores.
 *
 * Executa no core 0 com prioridade alta para garantir leituras consistentes.
 *
 * @param pvParameters Parâmetros da tarefa (não utilizados).
 */
void sensorTaskFunc(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    LOG_DEBUG(MODULE_NAME, "Tarefa de sensores iniciada (Core %d)", xPortGetCoreID());

    // Espera para garantir que todas as inicializações foram concluídas
    vTaskDelay(pdMS_TO_TICKS(200));

    while (true) {
        // Atualiza sensores
        if (g_sensorMutex != nullptr &&
            xSemaphoreTake(g_sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            g_sensorManager->update();
            xSemaphoreGive(g_sensorMutex);
        }

        // Atualiza monitor do sistema
        SystemMonitor::getInstance().update();

        // Executa no intervalo definido (preciso)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        #if defined(WOKWI_ENV) || defined(WOKWI)
            // No Wokwi, adicionamos um pequeno delay adicional para
            // evitar sobrecarga do simulador
            vTaskDelay(pdMS_TO_TICKS(5));
        #endif
    }
}

/**
 * Tarefa responsável pela interface web.
 *
 * Executa no core 1 para não interferir com a leitura dos sensores.
 *
 * @param pvParameters Parâmetros da tarefa (não utilizados).
 */
void webTaskFunc(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t counter = 0;

    LOG_DEBUG(MODULE_NAME, "Tarefa web iniciada (Core %d)", xPortGetCoreID());

    // Espera para garantir que todas as inicializações foram concluídas
    vTaskDelay(pdMS_TO_TICKS(500));

    while (true) {
        // Atualiza interface web
        if (g_sensorMutex != nullptr &&
            xSemaphoreTake(g_sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            g_webServer->update();
            xSemaphoreGive(g_sensorMutex);
        }

        // Verifica conexão WiFi periodicamente
        if (counter % 100 == 0) { // A cada 1 segundo
            WiFiManager::getInstance().update();
        }

        // Imprime estatísticas de memória a cada 10 segundos
        if (counter % 1000 == 0) { // A cada 10 segundos
            if (DEBUG_MEMORY) {
                MemoryManager::getInstance().printStats();
            }
        }

        counter++;

        // Executa no intervalo definido (preciso)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        #if defined(WOKWI_ENV) || defined(WOKWI)
            // No Wokwi, adicionamos um pequeno delay adicional para
            // evitar sobrecarga do simulador
            vTaskDelay(pdMS_TO_TICKS(5));
        #endif
    }
}

// Função para aguardar conexão WiFi com timeout
bool waitForWiFiConnection(uint32_t timeout_ms) {
    if (g_wifiConnectedSemaphore == nullptr) return false;

    // Se o WiFi já estiver conectado devido à inicialização antecipada
    // verificamos se podemos sinalizar o semáforo diretamente
    if (WiFi.status() == WL_CONNECTED && g_wifiEarlyInitDone && g_wifiEarlyInitSuccess) {
        LOG_INFO(MODULE_NAME, "WiFi já conectado - verificando semáforo");

        // Tentamos obter o semáforo com timeout mínimo para ver se já foi sinalizado
        if (xSemaphoreTake(g_wifiConnectedSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Já está sinalizado, devolvemos imediatamente
            LOG_INFO(MODULE_NAME, "Semáforo já sinalizado anteriormente");
            return true;
        }

        // Não está sinalizado ainda, sinalizamos usando o método do WifiPerformance
        LOG_INFO(MODULE_NAME, "Solicitando sinalização do semáforo ao módulo de performance");
        WifiPerformanceInitializer::signalWiFiSemaphore(g_wifiConnectedSemaphore);

        // Verificamos novamente com timeout mínimo
        return xSemaphoreTake(g_wifiConnectedSemaphore, pdMS_TO_TICKS(10)) == pdTRUE;
    }

    // Método tradicional: aguarda o semáforo ser sinalizado por um handler de evento
    LOG_INFO(MODULE_NAME, "Aguardando sinalização do semáforo (timeout: %ums)...", timeout_ms);
    return xSemaphoreTake(g_wifiConnectedSemaphore, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void setup() {
    // Inicializa a comunicação serial
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500); // Pequeno delay para estabilização

    // Inicialização do sistema de logging já realizada pelo include

    // Banner de inicialização
    LOG_INFO(MODULE_NAME, "===========================================");
    LOG_INFO(MODULE_NAME, "Sistema de Monitoramento do Solo v%s", FIRMWARE_VERSION);
    LOG_INFO(MODULE_NAME, "===========================================");

    // Delay para estabilização da saída serial
    delay(100);

    // Verifica e inicializa compatibilidade para Wokwi com saída formatada
    #if defined(WOKWI_ENV) || defined(WOKWI)
        LOG_INFO(MODULE_NAME, "Inicializando Ambiente Wokwi");
        WokwiCompat::init();
    #endif

    // Inicializa hardware
    Hardware::setupPins();

    // Configura a frequência da CPU
    #if defined(WOKWI_ENV) || defined(WOKWI)
        // Frequência reduzida para melhor desempenho no simulador
        setCpuFrequencyMhz(80);
    #else
        // Máxima velocidade para hardware real
        setCpuFrequencyMhz(240);
    #endif

    // Inicializa componentes - ORDEM É IMPORTANTE
    // 1. Primeiramente os serviços de sistema
    SystemMonitor::getInstance().init();
    MemoryManager::getInstance().init();
    OutputManager::initialize();
    OutputManager::attachConsoleManager(&ConsoleManager::getInstance());

    // 2. Cria semáforos antes de qualquer coisa que dependa deles
    g_sensorMutex = xSemaphoreCreateMutex();
    if (g_sensorMutex == nullptr) {
        LOG_FATAL(MODULE_NAME, "ERRO: Falha ao criar semáforo de sensores!");
        while (true) {
            delay(1000);
        }
    }

    // Cria semáforo binário para sinalização de WiFi
    g_wifiConnectedSemaphore = xSemaphoreCreateBinary();
    if (g_wifiConnectedSemaphore == nullptr) {
        LOG_FATAL(MODULE_NAME, "ERRO: Falha ao criar semáforo WiFi!");
        while (true) {
            delay(1000);
        }
    }

    // Inicializa o módulo de performance WiFi
    LOG_INFO(MODULE_NAME, "Inicializando módulo WiFi Performance");
    if (WifiPerformanceInitializer::getInstance().begin()) {
        LOG_INFO(MODULE_NAME, "Módulo de performance WiFi inicializado explicitamente");
    } else {
        LOG_ERROR(MODULE_NAME, "Falha ao inicializar módulo de performance WiFi");
    }

    // 3. Inicializa sensor manager (que inicializará IrrigationController internamente)
    g_sensorManager = new SensorManager();
    if (g_sensorManager) {
        g_sensorManager->init();
    } else {
        LOG_FATAL(MODULE_NAME, "ERRO: Falha ao criar SensorManager!");
        while (true) {
            delay(1000);
        }
    }

    // 4. Registra handler de eventos WiFi apenas se necessário
    // Se o WiFi já estiver inicializado pelo módulo de performance,
    // não registra novamente para evitar conflitos
    if (!g_wifiEarlyInitDone) {
        Serial.println("WiFi não inicializado pelo módulo de performance, registrando handler local");
        WiFi.onEvent(wifiEventHandler);
    } else {
        Serial.println("WiFi já inicializado pelo módulo de performance, usando handler existente");
    }

    // Delay para separação clara entre seções de mensagens
    delay(200);

    // Verifica o status da inicialização antecipada com formatação adequada
    LOG_INFO(MODULE_NAME, "Status WiFi - Inicialização antecipada: %s",
             g_wifiEarlyInitDone ? "Concluída" : "Não executada");

    if (g_wifiEarlyInitDone) {
        LOG_INFO(MODULE_NAME, "Resultado: %s",
                 g_wifiEarlyInitSuccess ? "Sucesso" : "Falha");
    }

    LOG_DEBUG(MODULE_NAME, "Status atual do WiFi: %d (WL_CONNECTED=%d)", WiFi.status(), WL_CONNECTED);

    if (WiFi.status() == WL_CONNECTED) {
        // WiFi já está conectado pelo inicializador antecipado
        LOG_INFO(MODULE_NAME, "WiFi já está conectado pelo módulo de performance");
        {
            IPAddress ip = WiFi.localIP();
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            LOG_INFO(MODULE_NAME, "IP: %s", ipStr);
        }

        // Força liberação do semáforo para não bloquear a inicialização
        if (g_wifiConnectedSemaphore != nullptr) {
            xSemaphoreGive(g_wifiConnectedSemaphore);
        }
    } else if (g_wifiEarlyInitDone) {
        // A inicialização antecipada foi tentada mas falhou
        LOG_WARN(MODULE_NAME, "Inicialização antecipada falhou, tentando novamente...");

        WiFi.disconnect(true);
        delay(100);

        // Conecta ao WiFi - diferente para Wokwi e hardware real
        #if defined(WOKWI_ENV) || defined(WOKWI)
            // Usa a função otimizada para ambiente Wokwi
            WokwiCompat::connectWiFi(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECTION_TIMEOUT);
        #else
            LOG_INFO(MODULE_NAME, "Conectando ao WiFi %s...", WIFI_SSID);
            WiFiManager::getInstance().connect(WIFI_SSID, WIFI_PASSWORD);
        #endif
    } else {
        // A inicialização antecipada não foi executada
        LOG_INFO(MODULE_NAME, "Inicializando WiFi pela primeira vez...");

        // Conecta ao WiFi - diferente para Wokwi e hardware real
        #if defined(WOKWI_ENV) || defined(WOKWI)
            // Usa a função otimizada para ambiente Wokwi
            WokwiCompat::connectWiFi(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECTION_TIMEOUT);
        #else
            LOG_INFO(MODULE_NAME, "Conexão WiFi Inicial para %s...", WIFI_SSID);
            WiFiManager::getInstance().connect(WIFI_SSID, WIFI_PASSWORD);
        #endif
    }

    // 5. Aguarda conexão WiFi com timeout antes de iniciar WebServer
    // Delay suficiente para garantir sincronização das mensagens do handler
    delay(300);

    // Espera pelo semáforo de conexão WiFi com formatação adequada
    LOG_INFO(MODULE_NAME, "Aguardando confirmação da conexão WiFi...");
    if (waitForWiFiConnection(WIFI_CONNECTION_TIMEOUT)) {
        LOG_INFO(MODULE_NAME, "WiFi pronto para comunicação!");
    } else {
        LOG_WARN(MODULE_NAME, "Timeout na conexão WiFi. Continuando inicialização...");
    }

    // 6. Agora que WiFi está pronto, inicializa WebServer
    delay(200); // Pequeno delay para estabilização

    g_webServer = new AsyncSoilWebServer(WEB_SERVER_PORT, *g_sensorManager);
    if (g_webServer) {
        g_webServer->begin();

        // Conecta o WebServer ao OutputManager para receber telemetria
        OutputManager::attachWebSocketServer(g_webServer);
    } else {
        LOG_FATAL(MODULE_NAME, "ERRO: Falha ao criar WebServer!");
        while (true) {
            delay(1000);
        }
    }

    // 7. Pequeno delay para estabilização antes de criar tarefas
    delay(300);

    // 8. Finalmente, cria tarefas FreeRTOS com tamanhos de stack adequados
    #if defined(WOKWI_ENV) || defined(WOKWI)
        // Stacks maiores para o simulador Wokwi
        xTaskCreatePinnedToCore(
            sensorTaskFunc,
            "SensorTask",
            8192,               // Stack dobrado para Wokwi
            NULL,
            TASK_PRIORITY_SENSOR,
            &g_sensorTask,
            TASK_SENSOR_CORE
        );

        xTaskCreatePinnedToCore(
            webTaskFunc,
            "WebTask",
            8192,               // Stack dobrado para Wokwi
            NULL,
            TASK_PRIORITY_WEB,
            &g_webTask,
            TASK_WEB_CORE
        );
    #else
        // Tamanhos normais para hardware real
        xTaskCreatePinnedToCore(
            sensorTaskFunc,
            "SensorTask",
            TASK_STACK_SIZE,
            NULL,
            TASK_PRIORITY_SENSOR,
            &g_sensorTask,
            TASK_SENSOR_CORE
        );

        xTaskCreatePinnedToCore(
            webTaskFunc,
            "WebTask",
            TASK_STACK_SIZE,
            NULL,
            TASK_PRIORITY_WEB,
            &g_webTask,
            TASK_WEB_CORE
        );
    #endif

    // Pequeno delay para garantir sequência correta das mensagens
    delay(100);

    // Mensagem de inicialização completa formatada
    LOG_INFO(MODULE_NAME, "===========================================");
    LOG_INFO(MODULE_NAME, "Sistema inicializado com sucesso!");
    LOG_INFO(MODULE_NAME, "===========================================");
}

void loop() {
    // O loop principal está vazio pois usamos FreeRTOS para tarefas
    // Esta função é executada na tarefa do Arduino no Core 1

    // O monitoramento do sistema e sensores agora é feito
    // diretamente pelo SensorManager com seu método de
    // atualização na mesma linha que cicla entre diferentes
    // visualizações de dados

    delay(1000);
}
