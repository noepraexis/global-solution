#!/usr/bin/env python3
"""
Demonstração do info_compile sem usar a API OpenAI
Mostra a capacidade de extração de conteúdo
"""

from info_compile import GPTURLAnalyzer
import json
from datetime import datetime

def demo_extraction():
    """Demonstra a extração de conteúdo sem usar a API"""
    
    # Criar analisador (API key fake apenas para demonstração)
    analyzer = GPTURLAnalyzer("demo-key", max_images=10)
    
    print("🔍 Demonstração de Extração de Conteúdo\n")
    
    # URLs relevantes para o Predicta
    test_urls = [
        ("GDACS - Sistema Global de Alertas", "https://www.gdacs.org/"),
        ("ReliefWeb - Desastres", "https://reliefweb.int/disasters"),
        ("INPE - Queimadas", "https://queimadas.dgi.inpe.br/queimadas/portal"),
    ]
    
    for name, url in test_urls[:1]:  # Testar apenas o primeiro
        print(f"\n{'='*60}")
        print(f"📍 {name}")
        print(f"🔗 {url}")
        print('='*60)
        
        try:
            # Extrair conteúdo
            text, images = analyzer.fetch_url_content(url)
            
            print(f"\n📄 TEXTO EXTRAÍDO:")
            print(f"   Tamanho: {len(text)} caracteres")
            print(f"   Preview: {text[:200]}...")
            
            # Análise de palavras-chave (sem API)
            keywords = ['flood', 'earthquake', 'cyclone', 'disaster', 'emergency', 
                       'alert', 'warning', 'evacuate', 'damage', 'casualties']
            found_keywords = [kw for kw in keywords if kw.lower() in text.lower()]
            
            if found_keywords:
                print(f"\n🔑 PALAVRAS-CHAVE ENCONTRADAS:")
                for kw in found_keywords:
                    print(f"   • {kw}")
            
            print(f"\n🖼️  IMAGENS ENCONTRADAS: {len(images)}")
            
            # Agrupar por tipo
            by_type = {}
            for img in images:
                img_type = img['type']
                by_type[img_type] = by_type.get(img_type, [])
                by_type[img_type].append(img)
            
            for img_type, imgs in by_type.items():
                print(f"\n   {img_type.upper()} ({len(imgs)}):")
                for i, img in enumerate(imgs[:3]):
                    if img.get('url'):
                        # Verificar se é mapa ou gráfico pela URL
                        url_lower = img['url'].lower()
                        if any(x in url_lower for x in ['map', 'chart', 'graph', 'plot']):
                            print(f"     📊 {img['url']}")
                        elif any(x in url_lower for x in ['.png', '.jpg', '.gif']):
                            print(f"     🖼️  {img['url']}")
                        else:
                            print(f"     🔗 {img['url']}")
                    else:
                        print(f"     📦 {img.get('alt', 'Embedded content')}")
                    
                    # Mostrar contexto se disponível
                    if img.get('context'):
                        print(f"        Contexto: {img['context'][:50]}...")
            
            # Simular análise (sem API)
            print(f"\n📊 ANÁLISE SIMULADA (sem API):")
            
            # Detectar possíveis alertas no texto
            alert_indicators = ['alert', 'warning', 'emergency', 'urgent', 'critical']
            alerts_found = sum(1 for indicator in alert_indicators if indicator in text.lower())
            
            if alerts_found > 0:
                print(f"   ⚠️  Possíveis alertas detectados: {alerts_found}")
            
            # Detectar números (possíveis estatísticas)
            import re
            numbers = re.findall(r'\b\d+[,.]?\d*\b', text)
            if numbers:
                print(f"   📈 Números encontrados: {len(numbers)}")
                print(f"      Exemplos: {', '.join(numbers[:5])}...")
            
            # Detectar datas
            date_patterns = re.findall(r'\b\d{1,2}[-/]\d{1,2}[-/]\d{2,4}\b|\b\d{4}[-/]\d{1,2}[-/]\d{1,2}\b', text)
            if date_patterns:
                print(f"   📅 Datas encontradas: {len(date_patterns)}")
                print(f"      Exemplos: {', '.join(date_patterns[:3])}...")
            
            # Salvar extração
            extraction_data = {
                'url': url,
                'name': name,
                'timestamp': datetime.now().isoformat(),
                'text_length': len(text),
                'images_found': len(images),
                'image_types': {k: len(v) for k, v in by_type.items()},
                'keywords_found': found_keywords,
                'alerts_detected': alerts_found,
                'numbers_found': len(numbers),
                'dates_found': len(date_patterns)
            }
            
            filename = f"extraction_{url.split('/')[2].replace('.', '_')}.json"
            with open(filename, 'w', encoding='utf-8') as f:
                json.dump(extraction_data, f, ensure_ascii=False, indent=2)
            
            print(f"\n💾 Dados salvos em: {filename}")
            
        except Exception as e:
            print(f"\n❌ Erro: {str(e)}")

def analyze_image_patterns():
    """Analisa padrões de imagens em sites de desastres"""
    
    analyzer = GPTURLAnalyzer("demo-key", max_images=20)
    
    print("\n\n🎯 Análise de Padrões de Imagens\n")
    
    # Sites conhecidos com mapas e visualizações
    map_sites = [
        "https://firms.modaps.eosdis.nasa.gov/map/",
        "https://worldview.earthdata.nasa.gov/",
        "https://www.windy.com/"
    ]
    
    for url in map_sites[:1]:
        print(f"\nAnalisando padrões em: {url}")
        try:
            _, images = analyzer.fetch_url_content(url)
            
            # Classificar imagens por padrão
            patterns = {
                'maps': [],
                'charts': [],
                'logos': [],
                'photos': [],
                'icons': [],
                'other': []
            }
            
            for img in images:
                url_str = str(img.get('url', '')).lower()
                alt_str = str(img.get('alt', '')).lower()
                context_str = str(img.get('context', '')).lower()
                
                # Classificar baseado em padrões
                if any(x in url_str + alt_str for x in ['map', 'mapa', 'cartograph']):
                    patterns['maps'].append(img)
                elif any(x in url_str + alt_str for x in ['chart', 'graph', 'plot']):
                    patterns['charts'].append(img)
                elif any(x in url_str + alt_str for x in ['logo', 'brand']):
                    patterns['logos'].append(img)
                elif any(x in url_str + alt_str for x in ['photo', 'image', 'picture']):
                    patterns['photos'].append(img)
                elif any(x in url_str + context_str for x in ['icon', 'button', 'arrow']):
                    patterns['icons'].append(img)
                else:
                    patterns['other'].append(img)
            
            # Mostrar estatísticas
            print("\n📊 Classificação de Imagens:")
            for category, imgs in patterns.items():
                if imgs:
                    print(f"   {category.upper()}: {len(imgs)}")
                    if category in ['maps', 'charts']:
                        for img in imgs[:2]:
                            print(f"      • {img.get('url', 'embedded')[:60]}...")
            
        except Exception as e:
            print(f"   Erro: {e}")

def create_integration_example():
    """Cria exemplo de integração com outros módulos do Predicta"""
    
    print("\n\n🔗 Exemplo de Integração com Predicta\n")
    
    integration_code = '''
# Integração do info_compile com outros módulos do Predicta

from info_compile import GPTURLAnalyzer
from gdacs.gdacs_api import GDACSClient
from glide.find_incident import GLIDESearch
from disaster_charter import DisasterCharterAPI

class PrediictaAnalyzer:
    """Analisador integrado do Predicta"""
    
    def __init__(self, openai_key):
        self.url_analyzer = GPTURLAnalyzer(openai_key)
        self.gdacs = GDACSClient()
        self.glide = GLIDESearch()
        self.charter = DisasterCharterAPI()
    
    def analyze_disaster_event(self, glide_number):
        """Análise completa de um evento de desastre"""
        
        # 1. Buscar informações básicas do GLIDE
        glide_info = self.glide.find_by_number(glide_number)
        
        # 2. Buscar alertas relacionados no GDACS
        gdacs_alerts = self.gdacs.get_alerts_by_country(glide_info['country'])
        
        # 3. Verificar ativações do Disaster Charter
        charter_images = self.charter.get_activation_images(glide_info['date'])
        
        # 4. Analisar páginas web relacionadas com info_compile
        analysis_results = []
        
        # Analisar página do GLIDE
        if glide_info.get('url'):
            glide_analysis = self.url_analyzer.analyze_url(
                glide_info['url'],
                custom_prompt="Extraia detalhes do desastre, impactos e resposta"
            )
            analysis_results.append(('GLIDE', glide_analysis))
        
        # Analisar dashboards do GDACS
        for alert in gdacs_alerts[:3]:
            if alert.get('dashboard_url'):
                gdacs_analysis = self.url_analyzer.analyze_url(
                    alert['dashboard_url'],
                    custom_prompt="Foque em mapas, estatísticas e evolução temporal"
                )
                analysis_results.append(('GDACS', gdacs_analysis))
        
        # Analisar imagens do Disaster Charter
        for img_url in charter_images[:2]:
            charter_analysis = self.url_analyzer.analyze_url(
                img_url,
                custom_prompt="Analise mudanças no terreno e extensão de danos"
            )
            analysis_results.append(('Charter', charter_analysis))
        
        return {
            'glide_number': glide_number,
            'sources_analyzed': len(analysis_results),
            'integrated_analysis': analysis_results
        }

# Uso:
# analyzer = PrediictaAnalyzer(os.getenv('OPENAI_API_KEY'))
# result = analyzer.analyze_disaster_event('FL-2024-000123-BRA')
'''
    
    print(integration_code)
    
    # Salvar exemplo
    with open('integration_example.py', 'w') as f:
        f.write(integration_code)
    
    print("\n💾 Exemplo de integração salvo em: integration_example.py")

if __name__ == "__main__":
    print("🌊 Predicta - Demonstração do info_compile\n")
    
    # Executar demonstrações
    demo_extraction()
    analyze_image_patterns()
    create_integration_example()
    
    print("\n\n✅ Demonstração concluída!")
    print("\n⚠️  Nota: Esta é uma demonstração sem usar a API OpenAI.")
    print("    Para análise completa com GPT-4 Vision, configure:")
    print("    export OPENAI_API_KEY='sua-chave-com-creditos'")