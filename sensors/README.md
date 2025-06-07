# Sistema de Prevenção de Desastres Naturais com ESP32

## 📖 Sobre este módulo do projeto

Este módulo (`/sensors`) lida com um dispositivo IoT (Internet das Coisas) focado na prevenção de desastres naturais. O sistema utiliza um microcontrolador ESP32 para coletar dados ambientais, como temperatura e umidade, que são cruciais para a identificação de riscos como enchentes ou incêndios florestais.

O dispositivo envia os dados coletados para uma API central, que seria responsável por analisar as informações e gerar alertas.

## ✅ Status das Principais Tarefas deste Módulo

As quatro metas técnicas iniciais para o desenvolvimento deste módulo foram concluídas com sucesso:

### 1. Definir os sensores adequados
- **Status:** ✅ Concluída
- **Detalhes:** Foi escolhido o sensor **DHT22** para medir temperatura e umidade do ar. Esses dados são essenciais para monitorar condições que podem levar a desastres.

### 2. Montar o circuito de sensores (virtual)
- **Status:** ✅ Concluída
- **Detalhes:** O circuito foi projetado e simulado na plataforma **Wokwi**. Ele inclui o ESP32, o sensor DHT22 e um LED para alertas visuais.

### 3. Escrever o código de leitura dos sensores
- **Status:** ✅ Concluída
- **Detalhes:** O software foi desenvolvido em **C++** com PlatformIO. O módulo `SensorManager` é responsável por fazer a leitura contínua dos dados, aplicando filtros para garantir a qualidade das medições.

### 4. Transmitir os dados para o sistema principal
- **Status:** ✅ Concluída
- **Detalhes:** Esta é a funcionalidade central do projeto. Foi criado um **`ApiClient`** que formata os dados dos sensores em **JSON** e os envia para uma API externa através de uma requisição `HTTP POST`.

**Exemplo do JSON enviado para a API**:
```json
{
  "temperatura": 29.5,
  "umidade": 45.1,
  "timestamp": 1677611200
}
```

---

## ⚙️ Funcionamento do Módulo

Este módulo executa os seguintes passos em ciclo contínuo:

1.  **Leitura**: O `SensorManager` lê os valores de temperatura e umidade do sensor DHT22 em intervalos regulares.
2.  **Conexão**: O `WiFiManager` conecta o ESP32 a uma rede Wi-Fi e gerencia a reconexão automática em caso de falha.
3.  **Transmissão**: O `ApiClient` pega os dados mais recentes, monta o payload JSON e os envia para o endpoint da API configurado.
4.  **Monitoramento Local (Opcional)**: Um servidor web embarcado (`AsyncWebServer`) permite visualizar os dados em tempo real através de um navegador, acessando o IP do dispositivo na rede local.

---

## 🔧 Hardware e Pinos

| Componente | Pino ESP32 | Função |
| :--- | :--- | :--- |
| **DHT22** (Data) | `GPIO23` |	Medição de temperatura e umidade |
| **LED** (Indicador) | `GPIO21` | Indicação de status ou alerta |

---

## 🚀 Como Executar este Módulo Localmente

### Pré-requisitos
- [Visual Studio Code](https://code.visualstudio.com/)
- Extensão [PlatformIO IDE](https://platformio.org/platformio-ide)

### Passos
1.  **Clone o repositório** e abra a pasta `/sensors` no VS Code.
2.  **Configure o WiFi e a API**: Altere as credenciais do WiFi (`WIFI_SSID`, `WIFI_PASSWORD`) e o endereço da sua API (`API_ENDPOINT_URL`) no arquivo `include/Config.h`.
3.  **Compile e Envie**: Use os botões do PlatformIO na barra de status (`Build`, `Upload`) para carregar o código no seu ESP32.
4.  **Monitore**: Abra o `Serial Monitor` para acompanhar os logs e ver o endereço IP do dispositivo.

### Simulação no Wokwi
Este módulo é compatível com o simulador **Wokwi**. Basta carregar os arquivos do projeto. O código irá se adaptar automaticamente ao ambiente de simulação. Você pode clicar no sensor DHT22 para alterar os valores e testar o sistema.
