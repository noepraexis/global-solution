# CLAUDE.md - Contexto do Projeto GPT URL Analyzer

## 📋 Visão Geral do Projeto

Este projeto desenvolve uma ferramenta Python avançada que utiliza a API do GPT-4 Vision para analisar páginas web de forma completa, extraindo e estruturando tanto conteúdo textual quanto visual em DataFrames pandas para análise posterior.

## 🎯 Intenção Principal

Criar uma solução robusta que permita:

1. **Extração Inteligente**: Capturar TODO o conteúdo relevante de uma página web, incluindo texto e elementos visuais (imagens, gráficos, infográficos, SVGs)

2. **Análise com IA**: Utilizar GPT-4 Vision para interpretar e estruturar o conteúdo, incluindo:
   - OCR de texto em imagens
   - Interpretação de gráficos e visualizações
   - Extração de dados de tabelas visuais
   - Compreensão contextual do conteúdo

3. **Estruturação em DataFrame**: Organizar todas as informações extraídas em formato tabular para facilitar análises, visualizações e integrações posteriores

## 🔧 Requisitos Técnicos

### Dependências Principais
- `openai` - API do GPT-4 Vision
- `pandas` - Estruturação de dados
- `beautifulsoup4` + `lxml` - Parsing HTML
- `requests` - Requisições HTTP
- `Pillow` - Processamento de imagens

### APIs Utilizadas
- OpenAI GPT-4 Vision (modelo `gpt-4o`)
- Capacidade de análise multimodal (texto + imagem)

## 👥 Especialidades Necessárias

Para o desenvolvimento completo deste projeto, as seguintes especialidades devem ser aplicadas:

### 1. **Engenharia de Web Scraping**
- Implementar extração robusta de conteúdo web
- Lidar com sites dinâmicos e lazy loading
- Contornar limitações comuns (rate limiting, user agents)
- Otimizar performance para sites grandes

### 2. **Processamento de Imagens**
- Detectar imagens independente do formato ou método de disponibilização
- Processar diferentes formatos (JPEG, PNG, WebP, SVG, AVIF)
- Otimizar imagens para envio à API (redimensionamento, compressão)
- Implementar detecção por magic bytes e content-type

### 3. **Integração de APIs de IA**
- Otimizar prompts para GPT-4 Vision
- Gerenciar tokens e custos
- Implementar retry logic e tratamento de erros
- Estruturar respostas JSON de forma consistente

### 4. **Arquitetura de Software**
- Design de classes limpas e reutilizáveis
- Implementar padrões de projeto apropriados
- Criar interfaces intuitivas
- Garantir extensibilidade para novos tipos de análise

### 5. **Análise de Dados**
- Estruturar DataFrames de forma otimizada
- Implementar agregações e transformações úteis
- Facilitar exportação para diversos formatos
- Criar metadados ricos para rastreabilidade

### 6. **UX/DX (Developer Experience)**
- Logs informativos e progress tracking
- Mensagens de erro claras e acionáveis
- Documentação inline completa
- Exemplos práticos de uso

## 📊 Casos de Uso Prioritários

1. **E-commerce**: Extrair informações de produtos, preços (incluindo os em imagens), especificações
2. **Dashboards**: Capturar e interpretar gráficos, KPIs, visualizações de dados
3. **Sites de Notícias**: Extrair artigos com infográficos e imagens contextuais
4. **Documentação Técnica**: Processar diagramas, fluxogramas, screenshots
5. **Redes Sociais**: Analisar posts com imagens e texto integrado

## 🏗️ Arquitetura Atual

```
GPTURLAnalyzer
├── fetch_url_content()      # Extração de conteúdo web
├── process_image_data()     # Processamento de imagens
├── analyze_with_gpt_vision() # Análise com IA
├── json_to_dataframe()      # Estruturação de dados
└── analyze_url()            # Orquestração completa
```

## 🚀 Melhorias Futuras Sugeridas

### Curto Prazo
1. **Cache de Análises**: Evitar reprocessamento de URLs já analisadas
2. **Batch Processing**: Analisar múltiplas URLs em paralelo
3. **Export Avançado**: Suporte para Excel com formatação, JSON estruturado
4. **Filtragem Inteligente**: Permitir focar em tipos específicos de conteúdo

### Médio Prazo
1. **Suporte a JavaScript**: Integração com Selenium/Playwright para SPAs
2. **Análise de Vídeo**: Extrair frames principais e analisar
3. **Multi-idioma**: Detecção e tradução automática
4. **API REST**: Expor funcionalidade como serviço

### Longo Prazo
1. **ML Customizado**: Treinar modelos específicos para domínios
2. **Knowledge Graph**: Construir grafos de conhecimento das análises
3. **Real-time Monitoring**: Acompanhar mudanças em sites ao longo do tempo
4. **Integração com Data Warehouses**: Conexão direta com BigQuery, Snowflake

## 💡 Diretrizes de Desenvolvimento

1. **Priorizar Precisão**: Melhor extrair menos dados corretos do que muitos incorretos
2. **Pensar em Escala**: Código deve funcionar tanto para 1 quanto para 1000 URLs
3. **Custo-Benefício**: Balance entre qualidade da análise e custos de API
4. **Resiliência**: Falhas parciais não devem comprometer análise completa
5. **Transparência**: Usuário deve entender o que foi e não foi extraído

## 🐛 Problemas Conhecidos

1. **Limite de Tokens**: GPT tem limite de contexto, precisamos chunkar conteúdo grande
2. **Rate Limiting**: Tanto em sites quanto na API OpenAI
3. **Custo**: Análise de muitas imagens pode ser cara
4. **Sites Protegidos**: Cloudflare, captchas, autenticação

## 📈 Métricas de Sucesso

- **Cobertura**: % de conteúdo relevante extraído vs. disponível
- **Precisão**: Acurácia da interpretação de dados visuais
- **Performance**: Tempo médio de análise por página
- **Custo**: $ por página analisada
- **Usabilidade**: Facilidade de integração e uso

## 🤝 Contribuindo

Ao desenvolver este projeto, considere:

1. **Testes Abrangentes**: Cobrir diversos tipos de sites
2. **Documentação Rica**: Exemplos para cada caso de uso
3. **Performance**: Profile e otimize gargalos
4. **Segurança**: Sanitizar inputs, proteger API keys
5. **Acessibilidade**: Considerar análise de sites com foco em a11y

---

**Nota**: Este projeto visa democratizar a extração e análise de dados web, tornando informações visuais tão acessíveis quanto texto para análise computacional.
