
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
