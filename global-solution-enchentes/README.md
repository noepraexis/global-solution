#Global Solution 2025.1 - Prevenção de Enchentes com Análise Estatística em R

#Objetivo
Analisar a relação entre variáveis ambientais (chuva, nível do rio, umidade do solo e hora do dia) para simular situações de risco de enchentes.

#O que foi feito?
- Análise estatística descritiva
- Geração de gráficos:
  - Histograma da chuva
  - Gráfico de dispersão (chuva x nível do rio)
  - Boxplot da umidade do solo por hora
- Cálculo da correlação entre chuva e nível do rio

#Conclusões
- Existe uma correlação positiva entre chuva e nível do rio
- A umidade do solo varia com o horário, podendo ser útil para sensores de alerta

#Próximos passos
- Conectar com sensores reais via ESP32 e Python
- Gerar alertas com base em regras definidas por dados estatísticos