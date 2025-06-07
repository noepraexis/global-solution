# -------------------------------------------------------------
# Análise Estatística para Prevenção de Enchentes Usando Dados Simulados em R
# Global Solution - Prevenção de Enchentes
# -------------------------------------------------------------



# ---------------------------
# 1. Carregando os pacotes
# ---------------------------

# Pacote para gráficos
library(ggplot2)


# Pacote para manipulação de dados
library(dplyr)


#Pacote para criar endpoints de API com suas funções R
install.packages("plumber")



# ---------------------------
# 2. Gerando base de dados simulada
# ---------------------------

# Define uma semente para reproduzir os mesmos resultados aleatórios
set.seed(123)


# Define o número de observações
n <- 1000


# Cria um dataframe com dados simulados relacionados ao clima e enchentes
dados <- data.frame(
  chuva_mm_24h = runif(n, min = 0, max = 150),          # Chuva acumulada em 24h (mm)
  chuva_mm_1h = runif(n, min = 0, max = 80),            # Chuva intensa em 1h (mm)
  nivel_rio_m = runif(n, min = 0.5, max = 6.0),         # Nível do rio (metros)
  umidade_solo_porc = runif(n, min = 10, max = 100),    # Umidade do solo (%)
  hora_dia = sample(0:23, n, replace = TRUE)            # Hora do dia da medição
)

# ---------------------------
# 3. Análise estatística descritiva
# ---------------------------

# Mostra um resumo estatístico básico dos dados
summary(dados)

# ---------------------------
# 4. Gráficos e Análises
# ---------------------------

# 4.1 - Gráfico de histograma
# Mostra a distribuição da variável "chuva_mm_24h",
#indicando a frequência das diferentes faixas de chuva acumulada em 24 horas.


ggplot(dados, aes(x = chuva_mm_24h)) +
  geom_histogram(binwidth = 10, fill = "steelblue", color = "black") +
  labs(title = "Distribuição da Chuva (mm/24h)", x = "Chuva (mm)", y = "Frequência")

# 4.2 - Correlação entre chuva e nível do rio
# Calcula o grau de associação linear entre chuva acumulada em 24h e nível do rio

correlacao <- cor(dados$chuva_mm_24h, dados$nivel_rio_m)
print(paste("Correlação entre chuva em 24h e nível do rio:", round(correlacao, 2)))

# 4.3 - Gráfico de dispersão (scatter plot)
# Visualiza a relação entre "chuva_mm_24h" e "nivel_rio_m", mostrando a tendência da variável nível do rio conforme a chuva aumenta.

ggplot(dados, aes(x = chuva_mm_24h, y = nivel_rio_m)) +
  geom_point(alpha = 0.5) +  # Pontos com transparência
  geom_smooth(method = "lm", se = FALSE, color = "red") +  # Linha de tendência
  labs(title = "Chuva x Nível do Rio", x = "Chuva (mm/24h)", y = "Nível do Rio (m)")

# 4.4 - Boxplot (gráfico de caixa)
# Mostra a distribuição da umidade do solo agrupada por hora do dia, permitindo visualizar a mediana, quartis e possíveis outliers.

ggplot(dados, aes(x = as.factor(hora_dia), y = umidade_solo_porc)) +
  geom_boxplot(fill = "lightgreen") +
  labs(title = "Umidade do Solo por Hora do Dia", x = "Hora (0-23)", y = "Umidade (%)") +
  theme(axis.text.x = element_text(angle = 90, hjust = 1))  # Rotaciona os rótulos do eixo x

# ---------------------------
# 5. Criando variável de risco de enchente
# ---------------------------

# Atribui níveis de risco com base em critérios de chuva, nível do rio e umidade do solo
dados$risco_enchente <- ifelse(
  dados$chuva_mm_24h > 100 & dados$nivel_rio_m > 4.5 & dados$umidade_solo_porc > 80,
  "ALTO",
  ifelse(dados$chuva_mm_24h > 70, "MODERADO", "BAIXO")
)

# ---------------------------
# 6. Criando API com Plumber
# ---------------------------

# Carrega o pacote plumber para criar a API
library(plumber)

#* @apiTitle API de Risco de Enchentes

#* Verifica se a API está ativa
#* @get /health
function() {
  list(status = "ok")
}

#* Calcula o risco com base em variáveis
#* @param chuva_mm_24h - Quantidade de chuva em 24 horas
#* @param nivel_rio_m - Altura do nível do rio
#* @param umidade_solo_porc - Porcentagem de umidade do solo
#* @get /calcular_risco
function(chuva_mm_24h, nivel_rio_m, umidade_solo_porc) {
  
  # Converte os parâmetros para numérico
  chuva <- as.numeric(chuva_mm_24h)
  rio <- as.numeric(nivel_rio_m)
  umidade <- as.numeric(umidade_solo_porc)
  
  # Aplica a lógica de classificação de risco
  risco <- if (chuva > 100 && rio > 4.5 && umidade > 80) {
    "ALTO"
  } else if (chuva > 70) {
    "MODERADO"
  } else {
    "BAIXO"
  }
  
  # Retorna o nível de risco
  list(risco = risco)
}

