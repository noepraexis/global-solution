"""
Extractors para dados web - análise profunda de conteúdo
"""

import re
import requests
from bs4 import BeautifulSoup
from typing import Dict, Any, List, Optional
import logging
from datetime import datetime
from urllib.parse import urlparse


class WebDataExtractor:
    """Extrator avançado de dados de páginas web"""
    
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (compatible; DisasterAnalysisBot/1.0)'
        })
    
    def extract_from_url(self, url: str) -> Dict[str, Any]:
        """Extrai dados estruturados de uma URL"""
        try:
            response = self.session.get(url, timeout=10)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            
            # Determinar tipo de fonte
            domain = urlparse(url).netloc
            
            if 'reliefweb' in domain:
                return self._extract_reliefweb(soup)
            elif 'ifrc' in domain:
                return self._extract_ifrc(soup)
            else:
                return self._extract_generic(soup)
                
        except Exception as e:
            self.logger.error(f"Erro ao extrair {url}: {e}")
            return {'error': str(e)}
    
    def _extract_reliefweb(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extrai dados específicos do ReliefWeb"""
        data = {}
        
        # Procurar por números de afetados
        affected_pattern = re.compile(r'(\d+(?:,\d+)*)\s*(?:people|persons?|families)\s*affected', re.I)
        deaths_pattern = re.compile(r'(\d+)\s*(?:deaths?|killed|died)', re.I)
        displaced_pattern = re.compile(r'(\d+(?:,\d+)*)\s*(?:displaced|evacuated)', re.I)
        
        text = soup.get_text()
        
        # Extrair números
        affected_match = affected_pattern.search(text)
        if affected_match:
            data['affected_population'] = int(affected_match.group(1).replace(',', ''))
        
        deaths_match = deaths_pattern.search(text)
        if deaths_match:
            data['deaths'] = int(deaths_match.group(1).replace(',', ''))
        
        displaced_match = displaced_pattern.search(text)
        if displaced_match:
            data['displaced'] = int(displaced_match.group(1).replace(',', ''))
        
        # Procurar por dados meteorológicos
        rain_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*(?:mm|millimeters?)\s*(?:of\s*)?(?:rain|precipitation)', re.I)
        rain_match = rain_pattern.search(text)
        if rain_match:
            data['precipitation_mm'] = float(rain_match.group(1))
        
        # Localização
        location_headers = soup.find_all(['h2', 'h3'], string=re.compile('location|where', re.I))
        if location_headers:
            next_elem = location_headers[0].find_next_sibling()
            if next_elem:
                data['location_details'] = next_elem.get_text(strip=True)
        
        # Data do evento
        date_pattern = re.compile(r'(\d{1,2})\s*(January|February|March|April|May|June|July|August|September|October|November|December)\s*(\d{4})', re.I)
        date_match = date_pattern.search(text)
        if date_match:
            data['event_date_extracted'] = f"{date_match.group(3)}-{self._month_to_number(date_match.group(2))}-{date_match.group(1).zfill(2)}"
        
        return data
    
    def _extract_ifrc(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extrai dados específicos do IFRC"""
        data = {}
        
        # IFRC geralmente tem tabelas com dados estruturados
        tables = soup.find_all('table')
        for table in tables:
            rows = table.find_all('tr')
            for row in rows:
                cells = row.find_all(['td', 'th'])
                if len(cells) >= 2:
                    key = cells[0].get_text(strip=True).lower()
                    value = cells[1].get_text(strip=True)
                    
                    # Mapear campos conhecidos
                    if 'affected' in key:
                        try:
                            data['affected_population'] = int(re.sub(r'[^\d]', '', value))
                        except ValueError:
                            pass
                    elif 'death' in key or 'killed' in key:
                        try:
                            data['deaths'] = int(re.sub(r'[^\d]', '', value))
                        except ValueError:
                            pass
        
        return data
    
    def _extract_generic(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extração genérica para qualquer site"""
        data = {}
        
        # Meta tags
        og_description = soup.find('meta', property='og:description')
        if og_description:
            data['description'] = og_description.get('content', '')
        
        # Título
        title = soup.find('title')
        if title:
            data['page_title'] = title.get_text(strip=True)
        
        # Procurar por padrões numéricos comuns
        text = soup.get_text()
        
        # Temperatura
        temp_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*°?[CF]', re.I)
        temp_match = temp_pattern.search(text)
        if temp_match:
            temp_value = float(temp_match.group(1))
            if 'F' in temp_match.group(0):
                temp_value = (temp_value - 32) * 5/9  # Converter para Celsius
            data['temperature_c'] = temp_value
        
        # Velocidade do vento
        wind_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*(?:km/h|kmh|kilometers?\s*per\s*hour)', re.I)
        wind_match = wind_pattern.search(text)
        if wind_match:
            data['wind_speed_kmh'] = float(wind_match.group(1))
        
        return data
    
    def _month_to_number(self, month_name: str) -> str:
        """Converte nome do mês para número"""
        months = {
            'january': '01', 'february': '02', 'march': '03', 'april': '04',
            'may': '05', 'june': '06', 'july': '07', 'august': '08',
            'september': '09', 'october': '10', 'november': '11', 'december': '12'
        }
        return months.get(month_name.lower(), '01')
    
    def extract_sentiment(self, text: str) -> float:
        """Análise básica de sentimento (placeholder para ML mais avançado)"""
        # Palavras indicativas de severidade
        severe_words = ['catastrophic', 'devastating', 'severe', 'extreme', 'critical', 
                       'emergency', 'disaster', 'crisis', 'urgent', 'deadly']
        moderate_words = ['significant', 'moderate', 'considerable', 'affected']
        
        text_lower = text.lower()
        
        severe_count = sum(1 for word in severe_words if word in text_lower)
        moderate_count = sum(1 for word in moderate_words if word in text_lower)
        
        # Score simples: -1 (muito severo) a 0 (neutro)
        if severe_count > 3:
            return -1.0
        elif severe_count > 1:
            return -0.7
        elif moderate_count > 2:
            return -0.5
        else:
            return -0.3
