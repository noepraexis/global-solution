#!/usr/bin/env python3
"""
Exemplo de uso do info_compile para análise de desastres
Específico para o projeto Predicta
"""

import os
from info_compile import GPTURLAnalyzer
import json
from datetime import datetime

def analyze_disaster_dashboard():
    """Analisa dashboards de desastres com prompts customizados"""
    
    api_key = os.getenv('OPENAI_API_KEY')
    if not api_key:
        print("❌ Configure OPENAI_API_KEY primeiro!")
        return
    
    # Criar analisador otimizado para desastres
    analyzer = GPTURLAnalyzer(api_key, max_images=10)
    
    # Prompt específico para análise de desastres
    disaster_prompt = """
    Você está analisando uma página sobre desastres naturais. Extraia e estruture:
    
    1. INFORMAÇÕES CRÍTICAS:
       - Tipo de desastre (enchente, deslizamento, etc.)
       - Localização precisa (país, estado, cidade, coordenadas se disponível)
       - Data/hora do evento
       - Status atual (ativo, controlado, finalizado)
       - Severidade/magnitude
    
    2. IMPACTOS:
       - População afetada (números)
       - Área afetada (km²)
       - Infraestrutura danificada
       - Vítimas (feridos, mortos, desaparecidos)
       - Desabrigados/evacuados
    
    3. DADOS VISUAIS:
       - Mapas: identifique áreas de risco, zonas afetadas
       - Gráficos: extraia tendências, previsões
       - Imagens de satélite: mudanças no terreno, extensão de danos
       - Infográficos: estatísticas e comparações
    
    4. RESPOSTA E RECURSOS:
       - Organizações envolvidas
       - Recursos mobilizados
       - Áreas prioritárias
       - Rotas de evacuação
    
    5. PREVISÕES:
       - Evolução esperada
       - Riscos adicionais
       - Condições meteorológicas futuras
    
    Priorize informações quantitativas e geográficas precisas.
    """
    
    # URLs de exemplo para análise
    urls_analise = [
        # Coloque aqui URLs reais de dashboards de desastres
        "https://www.gdacs.org/",
        # "https://example.com/flood-dashboard",
        # "https://example.com/landslide-report"
    ]
    
    for url in urls_analise:
        print(f"\n{'='*60}")
        print(f"Analisando: {url}")
        print('='*60)
        
        try:
            # Executar análise
            df = analyzer.analyze_url(url, custom_prompt=disaster_prompt)
            
            # Salvar resultados
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_file = f"disaster_analysis_{timestamp}.csv"
            df.to_csv(output_file, index=False, encoding='utf-8')
            
            print(f"\n✓ Análise concluída!")
            print(f"  - Total de registros: {len(df)}")
            print(f"  - Arquivo salvo: {output_file}")
            
            # Mostrar resumo
            summary = df[df['tipo'] == 'resumo']['conteudo'].values
            if len(summary) > 0:
                print(f"\nResumo: {summary[0][:200]}...")
            
            # Mostrar insights principais
            insights = df[df['tipo'] == 'insight']['conteudo'].values
            if len(insights) > 0:
                print("\nPrincipais insights:")
                for i, insight in enumerate(insights[:3], 1):
                    print(f"  {i}. {insight}")
            
            # Análise de imagens
            img_analysis = df[df['tipo'] == 'imagem']
            if not img_analysis.empty:
                print(f"\nImagens analisadas: {len(img_analysis)}")
                for _, row in img_analysis.head(2).iterrows():
                    print(f"  - {row['conteudo'][:100]}...")
            
        except Exception as e:
            print(f"❌ Erro na análise: {str(e)}")

def analyze_specific_disaster(glide_number: str):
    """
    Analisa um desastre específico usando GLIDE number
    Integração com o módulo GLIDE do Predicta
    """
    
    # Exemplo de URL construída com GLIDE
    # Você pode integrar com o módulo glide/find_incident.py
    base_url = "https://glidenumber.net/glide/public/search/details.jsp?glide="
    url = f"{base_url}{glide_number}"
    
    api_key = os.getenv('OPENAI_API_KEY')
    if not api_key:
        return
    
    analyzer = GPTURLAnalyzer(api_key)
    
    prompt_glide = """
    Extraia informações do desastre identificado pelo GLIDE number:
    
    1. Identificação:
       - GLIDE Number
       - Tipo de desastre
       - País e região específica
       - Data do evento
    
    2. Detalhes do impacto:
       - Casualties (mortos/feridos/desaparecidos)
       - População afetada
       - Danos econômicos
       - Infraestrutura afetada
    
    3. Resposta:
       - Organizações respondendo
       - Ajuda internacional
       - Status atual
    
    Estruture os dados para fácil integração com sistemas de monitoramento.
    """
    
    try:
        df = analyzer.analyze_url(url, custom_prompt=prompt_glide)
        
        # Processar para formato Predicta
        disaster_data = {
            'glide_number': glide_number,
            'source': 'info_compile',
            'timestamp': datetime.now().isoformat(),
            'data': df.to_dict('records')
        }
        
        # Salvar JSON para integração
        with open(f'disaster_{glide_number}.json', 'w', encoding='utf-8') as f:
            json.dump(disaster_data, f, ensure_ascii=False, indent=2)
            
        print(f"✓ Dados do desastre {glide_number} extraídos")
        
    except Exception as e:
        print(f"❌ Erro ao analisar {glide_number}: {e}")

def monitor_realtime_alerts():
    """
    Exemplo de monitoramento contínuo de alertas
    Pode ser agendado com cron para execução periódica
    """
    
    # URLs de monitoramento em tempo real
    monitoring_urls = {
        'gdacs': 'https://www.gdacs.org/default.aspx',
        'reliefweb': 'https://reliefweb.int/disasters',
        'brasil': 'https://www.gov.br/mdr/pt-br/assuntos/protecao-e-defesa-civil'
    }
    
    api_key = os.getenv('OPENAI_API_KEY')
    if not api_key:
        return
        
    analyzer = GPTURLAnalyzer(api_key, max_images=5)
    
    alerts = []
    
    for source, url in monitoring_urls.items():
        try:
            print(f"\nMonitorando {source}...")
            
            prompt_monitoring = """
            Identifique APENAS alertas e eventos NOVOS ou ATIVOS:
            
            1. Alertas de desastres nas últimas 48 horas
            2. Situações de emergência em desenvolvimento
            3. Avisos meteorológicos severos
            4. Áreas em estado de alerta
            
            Para cada alerta, extraia:
            - Tipo e severidade
            - Localização específica
            - Tempo (quando começou/previsão)
            - População em risco
            - Ações recomendadas
            
            Ignore informações históricas ou eventos já finalizados.
            """
            
            df = analyzer.analyze_url(url, custom_prompt=prompt_monitoring)
            
            # Filtrar apenas alertas relevantes
            alert_df = df[df['tipo'].isin(['alerta', 'emergência', 'aviso'])]
            
            if not alert_df.empty:
                alerts.append({
                    'source': source,
                    'url': url,
                    'timestamp': datetime.now().isoformat(),
                    'alerts': alert_df.to_dict('records')
                })
                
                print(f"  ⚠️  {len(alert_df)} alertas encontrados!")
            else:
                print(f"  ✓ Nenhum alerta novo")
                
        except Exception as e:
            print(f"  ❌ Erro: {e}")
    
    # Salvar compilação de alertas
    if alerts:
        with open(f'alerts_{datetime.now().strftime("%Y%m%d_%H%M")}.json', 'w') as f:
            json.dump(alerts, f, ensure_ascii=False, indent=2)
        print(f"\n📊 Total: {len(alerts)} fontes com alertas ativos")

if __name__ == "__main__":
    print("🌊 Predicta - Análise de Desastres com info_compile\n")
    
    # Escolher modo de operação
    import sys
    
    if len(sys.argv) > 1:
        if sys.argv[1] == '--monitor':
            monitor_realtime_alerts()
        elif sys.argv[1].startswith('--glide='):
            glide = sys.argv[1].split('=')[1]
            analyze_specific_disaster(glide)
    else:
        # Modo padrão: análise de dashboards
        analyze_disaster_dashboard()
        
        print("\n💡 Outros modos disponíveis:")
        print("   python analyze_disaster.py --monitor")
        print("   python analyze_disaster.py --glide=FL-2024-000123-BRA")