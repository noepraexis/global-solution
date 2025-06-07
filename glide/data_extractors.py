"""
Extractors para dados web - análise profunda de conteúdo
Versão 2.0 - Melhorado com múltiplos padrões, fallbacks e validação
"""

import re
import requests
from bs4 import BeautifulSoup
from typing import Dict, Any, List, Optional, Tuple
import logging
from datetime import datetime
from urllib.parse import urlparse
import time
from dataclasses import dataclass, field
from concurrent.futures import ThreadPoolExecutor, as_completed


@dataclass
class ExtractionReport:
    """Relatório de extração com metadados"""
    url: str
    success: bool
    data: Dict[str, Any] = field(default_factory=dict)
    errors: List[str] = field(default_factory=list)
    confidence_scores: Dict[str, float] = field(default_factory=dict)
    extraction_time: float = 0.0


class WebDataExtractor:
    """Extrator avançado de dados de páginas web com fallbacks e validação"""
    
    def __init__(self, timeout: int = 15, max_retries: int = 3):
        self.logger = logging.getLogger(self.__class__.__name__)
        self.timeout = timeout
        self.max_retries = max_retries
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'pt-BR,pt;q=0.9,en;q=0.8'
        })
        
        # Cache para evitar re-processamento
        self.cache = {}
        
        # Compilar padrões para melhor performance
        self._compile_patterns()
    
    def _compile_patterns(self):
        """Compila padrões regex para melhor performance"""
        self.patterns = {
            'affected': [
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:pessoas?|people|persons?)\s*(?:foram\s*)?(?:afetad[ao]s?|affected)', re.I),
                re.compile(r'(?:afetou|affected)\s*(\d{1,3}(?:[.,]\d{3})*)\s*(?:pessoas?|people)', re.I),
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:moradores?|residents?)\s*(?:atingid[ao]s?|impacted)', re.I),
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:famílias?|families)\s*(?:afetad[ao]s?|affected)', re.I),
            ],
            'deaths': [
                re.compile(r'(\d+)\s*(?:mortes?|deaths?|óbitos?|fatalities|mortos?|killed|died)', re.I),
                re.compile(r'(?:morreram|died|killed)\s*(\d+)', re.I),
                re.compile(r'(\d+)\s*(?:vítimas?\s*fatais|fatal\s*victims?)', re.I),
            ],
            'displaced': [
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:desabrigad[ao]s?|displaced|homeless)', re.I),
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:evacuad[ao]s?|evacuated)', re.I),
                re.compile(r'(\d{1,3}(?:[.,]\d{3})*)\s*(?:desalojad[ao]s?|left\s*homeless)', re.I),
            ],
            'precipitation': [
                re.compile(r'(\d+(?:[.,]\d+)?)\s*(?:mm|milímetros?|millimeters?)\s*(?:de\s*)?(?:chuva|rain|precipita[çc][ãa]o)', re.I),
                re.compile(r'(?:chuva|rain|precipita[çc][ãa]o)\s*(?:de\s*)?(\d+(?:[.,]\d+)?)\s*(?:mm|milímetros?)', re.I),
                re.compile(r'(\d+(?:[.,]\d+)?)\s*(?:mm|milímetros?)\s*(?:em\s*)?(?:\d+\s*)?(?:horas?|hours?|dias?|days?)', re.I),
            ],
            'economic_loss': [
                re.compile(r'(?:prejuíz[oa]s?|loss|damage|danos?)\s*(?:de\s*)?(?:R\$|US\$|USD|BRL)\s*(\d+(?:[.,]\d+)?)\s*(?:milh[õo]es?|million|bilh[õo]es?|billion)', re.I),
                re.compile(r'(?:R\$|US\$|USD)\s*(\d+(?:[.,]\d+)?)\s*(?:milh[õo]es?|mi|million|bilh[õo]es?|bi|billion)\s*(?:em\s*)?(?:prejuíz[oa]s?|perdas?|danos?)', re.I),
            ],
            'water_level': [
                re.compile(r'(?:nível|level)\s*(?:d[aeo]\s*)?(?:água|water|rio|river)\s*(?:atingiu|reached|chegou)\s*(?:a\s*)?(\d+(?:[.,]\d+)?)\s*(?:metros?|meters?|m\b)', re.I),
                re.compile(r'(\d+(?:[.,]\d+)?)\s*(?:metros?|meters?|m)\s*(?:de\s*)?(?:água|water|inundação|flooding)', re.I),
            ]
        }
    
    def extract_from_url(self, url: str, force_refresh: bool = False) -> ExtractionReport:
        """Extrai dados estruturados de uma URL com relatório detalhado"""
        start_time = time.time()
        report = ExtractionReport(url=url, success=False)
        
        # Verificar cache
        if not force_refresh and url in self.cache:
            self.logger.info(f"Usando cache para {url}")
            return self.cache[url]
        
        try:
            # Buscar conteúdo com retry
            html_content = self._fetch_with_retry(url)
            if not html_content:
                report.errors.append("Falha ao buscar conteúdo da URL")
                return report
            
            soup = BeautifulSoup(html_content, 'html.parser')
            
            # Extrair texto limpo
            text_content = self._extract_clean_text(soup)
            
            # Determinar tipo de fonte
            domain = urlparse(url).netloc
            
            # Executar extração específica por domínio
            if 'reliefweb' in domain:
                report.data = self._extract_reliefweb(soup)
            elif 'ifrc' in domain:
                report.data = self._extract_ifrc(soup)
            else:
                report.data = self._extract_generic(soup)
            
            # Adicionar extração com padrões compilados para campos faltantes
            pattern_data = self._extract_with_patterns(text_content)
            for key, value in pattern_data.items():
                if key not in report.data or report.data[key] is None:
                    report.data[key] = value
            
            # Calcular scores de confiança
            report.confidence_scores = self._calculate_confidence_scores(report.data)
            
            # Validar e limpar dados
            report.data = self._validate_and_clean_data(report.data)
            
            # Adicionar metadados
            report.data['source_url'] = url
            report.data['extraction_timestamp'] = datetime.now().isoformat()
            report.data['source_domain'] = domain
            
            report.success = True
            report.extraction_time = time.time() - start_time
            
            # Cachear resultado
            self.cache[url] = report
            
        except Exception as e:
            self.logger.error(f"Erro ao extrair {url}: {str(e)}")
            report.errors.append(f"Erro de extração: {str(e)}")
        
        return report
    
    def _fetch_with_retry(self, url: str) -> Optional[str]:
        """Busca conteúdo com retry e tratamento de erros"""
        for attempt in range(self.max_retries):
            try:
                response = self.session.get(url, timeout=self.timeout)
                response.raise_for_status()
                
                # Detectar encoding correto
                if response.encoding is None:
                    response.encoding = response.apparent_encoding
                
                return response.text
                
            except requests.exceptions.Timeout:
                self.logger.warning(f"Timeout na tentativa {attempt + 1} para {url}")
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)  # Backoff exponencial
                    
            except requests.exceptions.RequestException as e:
                self.logger.error(f"Erro na requisição: {e}")
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)
        
        return None
    
    def _extract_clean_text(self, soup: BeautifulSoup) -> str:
        """Extrai texto limpo removendo scripts e estilos"""
        # Remover scripts e estilos
        for script in soup(["script", "style", "meta", "link"]):
            script.decompose()
        
        # Extrair texto
        text = soup.get_text(separator=' ', strip=True)
        
        # Limpar espaços múltiplos
        text = re.sub(r'\s+', ' ', text)
        
        return text
    
    def _extract_with_patterns(self, text: str) -> Dict[str, Any]:
        """Extrai dados usando padrões compilados"""
        results = {}
        
        # Tentar cada padrão em ordem de prioridade
        for field, patterns in self.patterns.items():
            for pattern in patterns:
                match = pattern.search(text)
                if match:
                    value = self._process_match(match, field)
                    if value is not None:
                        results[field] = value
                        break  # Usar primeiro match válido
        
        return results
    
    def _process_match(self, match: re.Match, field: str) -> Any:
        """Processa um match de regex baseado no tipo de campo"""
        try:
            if field in ['affected', 'deaths', 'displaced']:
                # Remover separadores de milhares e converter para int
                number_str = match.group(1).replace('.', '').replace(',', '')
                return int(number_str)
                
            elif field in ['precipitation', 'water_level']:
                # Converter para float
                number_str = match.group(1).replace(',', '.')
                return float(number_str)
                
            elif field == 'economic_loss':
                # Converter valores monetários
                number_str = match.group(1).replace(',', '.')
                value = float(number_str)
                
                # Aplicar multiplicador se presente
                text = match.group(0).lower()
                if 'bilh' in text or 'billion' in text:
                    value *= 1_000_000_000
                elif 'milh' in text or 'million' in text:
                    value *= 1_000_000
                
                return value
                
        except Exception as e:
            self.logger.debug(f"Erro ao processar match para {field}: {e}")
        
        return None
    
    def _extract_reliefweb(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extrai dados específicos do ReliefWeb com busca aprimorada"""
        data = {}
        text = soup.get_text()
        
        # Buscar em estruturas específicas do ReliefWeb
        # 1. Metadados estruturados
        meta_sections = soup.find_all('div', class_=['field', 'meta', 'report-meta'])
        for section in meta_sections:
            label = section.find(['dt', 'label', 'span'], class_=['label', 'field-label'])
            value = section.find(['dd', 'div', 'span'], class_=['value', 'field-content'])
            
            if label and value:
                label_text = label.get_text(strip=True).lower()
                value_text = value.get_text(strip=True)
                
                if 'location' in label_text:
                    data['location_details'] = value_text
                elif 'date' in label_text:
                    data['report_date'] = value_text
                elif 'source' in label_text:
                    data['source_organization'] = value_text
        
        # 2. Buscar tabelas de dados
        tables = soup.find_all('table')
        for table in tables:
            headers = [th.get_text(strip=True).lower() for th in table.find_all('th')]
            if any(keyword in ' '.join(headers) for keyword in ['affected', 'deaths', 'impact']):
                for row in table.find_all('tr')[1:]:  # Pular header
                    cells = row.find_all(['td', 'th'])
                    if len(cells) >= 2:
                        key = cells[0].get_text(strip=True).lower()
                        value = cells[1].get_text(strip=True)
                        
                        # Extrair números
                        number_match = re.search(r'(\d+(?:[.,]\d+)*)', value)
                        if number_match:
                            number = int(number_match.group(1).replace(',', '').replace('.', ''))
                            
                            if 'affected' in key:
                                data['affected_population'] = number
                            elif 'death' in key or 'killed' in key:
                                data['deaths'] = number
                            elif 'displaced' in key:
                                data['displaced'] = number
                            elif 'injured' in key:
                                data['injured'] = number
        
        # 3. Buscar em seções de conteúdo principal
        content_section = soup.find(['div', 'article'], class_=['content', 'report-content'])
        if content_section:
            content_text = content_section.get_text(separator=' ', strip=True)
            
            # Extrair coordenadas se presentes
            coord_match = re.search(r'(\-?\d+\.?\d*)[,\s]+(\-?\d+\.?\d*)', content_text)
            if coord_match:
                lat, lon = float(coord_match.group(1)), float(coord_match.group(2))
                if -90 <= lat <= 90 and -180 <= lon <= 180:
                    data['latitude'] = lat
                    data['longitude'] = lon
        
        # 4. Extrair dados de anexos/downloads se disponíveis
        downloads = soup.find_all('a', href=re.compile(r'\.(pdf|xlsx?|csv)', re.I))
        if downloads:
            data['has_attachments'] = True
            data['attachment_count'] = len(downloads)
        
        return data
    
    def _extract_ifrc(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extrai dados específicos do IFRC com busca aprimorada"""
        data = {}
        
        # 1. Buscar em info boxes / cards
        info_boxes = soup.find_all(['div', 'section'], class_=['infobox', 'emergency-info', 'key-figures'])
        
        for box in info_boxes:
            # Buscar pares de label/value
            labels = box.find_all(['dt', 'span', 'div'], class_=['label', 'metric-label'])
            values = box.find_all(['dd', 'span', 'div'], class_=['value', 'metric-value', 'number'])
            
            for label, value in zip(labels, values):
                label_text = label.get_text(strip=True).lower()
                value_text = value.get_text(strip=True)
                
                if 'people' in label_text or 'affected' in label_text:
                    number = self._extract_number_from_text(value_text)
                    if number:
                        data['affected_population'] = number
                elif 'death' in label_text:
                    number = self._extract_number_from_text(value_text)
                    if number:
                        data['deaths'] = number
                elif 'funding' in label_text or 'appeal' in label_text:
                    amount = self._extract_currency_amount(value_text)
                    if amount:
                        data['funding_required_usd'] = amount
        
        # 2. IFRC geralmente tem tabelas com dados estruturados
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
                    elif 'injured' in key:
                        try:
                            data['injured'] = int(re.sub(r'[^\d]', '', value))
                        except ValueError:
                            pass
                    elif 'displaced' in key or 'homeless' in key:
                        try:
                            data['displaced'] = int(re.sub(r'[^\d]', '', value))
                        except ValueError:
                            pass
        
        # 3. Buscar tipo de operação IFRC
        operation_type = soup.find(['span', 'div'], class_=['operation-type', 'appeal-type'])
        if operation_type:
            data['operation_type'] = operation_type.get_text(strip=True)
        
        return data
    
    def _extract_generic(self, soup: BeautifulSoup) -> Dict[str, Any]:
        """Extração genérica melhorada para qualquer site"""
        data = {}
        
        # 1. Extrair todos os metadados disponíveis
        # Open Graph
        og_tags = soup.find_all('meta', property=re.compile(r'^og:'))
        for tag in og_tags:
            prop = tag.get('property', '').replace('og:', '')
            content = tag.get('content', '')
            if prop and content:
                data[f'og_{prop}'] = content
        
        # Twitter Card
        twitter_tags = soup.find_all('meta', attrs={'name': re.compile(r'^twitter:')})
        for tag in twitter_tags:
            name = tag.get('name', '').replace('twitter:', '')
            content = tag.get('content', '')
            if name and content:
                data[f'twitter_{name}'] = content
        
        # Título e descrição
        title = soup.find('title')
        if title:
            data['page_title'] = title.get_text(strip=True)
        
        meta_desc = soup.find('meta', attrs={'name': 'description'})
        if meta_desc:
            data['description'] = meta_desc.get('content', '')
        
        # 2. Buscar em elementos semânticos HTML5
        main_content = soup.find(['main', 'article', 'div'], class_=['content', 'main-content', 'article-body'])
        if main_content:
            # Primeiros parágrafos geralmente têm resumo
            first_paragraphs = main_content.find_all('p')[:5]
            summary_text = ' '.join(p.get_text(strip=True) for p in first_paragraphs)
            
            if len(summary_text) > 100:
                data['summary'] = summary_text[:500]
        
        # 3. Buscar dados em listas estruturadas
        fact_lists = soup.find_all(['ul', 'ol'], class_=['facts', 'key-points', 'highlights'])
        if fact_lists:
            facts = []
            for lst in fact_lists:
                items = lst.find_all('li')
                facts.extend([item.get_text(strip=True) for item in items[:5]])
            if facts:
                data['key_facts'] = facts[:10]  # Limitar a 10 fatos
        
        # 4. Procurar por padrões numéricos comuns
        text = soup.get_text()
        
        # Temperatura com validação
        temp_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*°?[CF]', re.I)
        temp_match = temp_pattern.search(text)
        if temp_match:
            temp_value = float(temp_match.group(1))
            if 'F' in temp_match.group(0).upper():
                temp_value = (temp_value - 32) * 5/9  # Converter para Celsius
            if -50 <= temp_value <= 60:  # Validação de sanidade
                data['temperature_c'] = round(temp_value, 1)
        
        # Velocidade do vento
        wind_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*(?:km/h|kmh|kilometers?\s*per\s*hour)', re.I)
        wind_match = wind_pattern.search(text)
        if wind_match:
            wind_speed = float(wind_match.group(1))
            if 0 <= wind_speed <= 400:  # Validação
                data['wind_speed_kmh'] = round(wind_speed, 1)
        
        # Umidade
        humidity_pattern = re.compile(r'(\d+(?:\.\d+)?)\s*%\s*(?:humidity|umidade)', re.I)
        humidity_match = humidity_pattern.search(text)
        if humidity_match:
            humidity = float(humidity_match.group(1))
            if 0 <= humidity <= 100:
                data['humidity_percent'] = round(humidity, 1)
        
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
    
    def _extract_number_from_text(self, text: str) -> Optional[int]:
        """Extrai número de texto com tratamento robusto"""
        # Remover pontuação exceto vírgulas e pontos
        text = re.sub(r'[^\d.,\s]', ' ', text)
        
        # Padrões de números
        patterns = [
            r'(\d{1,3}(?:[.,]\d{3})+)',  # 1.234.567 ou 1,234,567
            r'(\d+)'  # Números simples
        ]
        
        for pattern in patterns:
            match = re.search(pattern, text)
            if match:
                number_str = match.group(1)
                # Normalizar separadores
                number_str = number_str.replace('.', '').replace(',', '')
                try:
                    return int(number_str)
                except ValueError:
                    continue
        
        return None
    
    def _extract_currency_amount(self, text: str) -> Optional[float]:
        """Extrai valores monetários e converte para USD"""
        patterns = [
            (r'(?:US\$|USD)\s*(\d+(?:[.,]\d+)?)\s*(million|billion)?', 1.0),
            (r'(?:R\$|BRL)\s*(\d+(?:[.,]\d+)?)\s*(milhões?|bilhões?)?', 0.2),  # Taxa aproximada
            (r'€\s*(\d+(?:[.,]\d+)?)\s*(million|billion)?', 1.1),  # Euro para USD
        ]
        
        for pattern, rate in patterns:
            match = re.search(pattern, text, re.I)
            if match:
                number_str = match.group(1).replace(',', '.')
                try:
                    value = float(number_str)
                    
                    # Aplicar multiplicador
                    if match.group(2):
                        multiplier_text = match.group(2).lower()
                        if 'million' in multiplier_text or 'milhõ' in multiplier_text:
                            value *= 1_000_000
                        elif 'billion' in multiplier_text or 'bilhõ' in multiplier_text:
                            value *= 1_000_000_000
                    
                    return value * rate
                except ValueError:
                    continue
        
        return None
    
    def _calculate_confidence_scores(self, data: Dict[str, Any]) -> Dict[str, float]:
        """Calcula scores de confiança para cada campo extraído"""
        scores = {}
        
        # Campos numéricos - verificar se estão em ranges razoáveis
        numeric_validations = {
            'affected_population': (0, 10_000_000),
            'deaths': (0, 10_000),
            'displaced': (0, 1_000_000),
            'injured': (0, 100_000),
            'precipitation_mm': (0, 2000),
            'temperature_c': (-50, 60),
            'wind_speed_kmh': (0, 400),
            'water_level': (0, 50),
            'economic_loss_usd': (0, 100_000_000_000)
        }
        
        for field, (min_val, max_val) in numeric_validations.items():
            if field in data and data[field] is not None:
                value = data[field]
                if min_val <= value <= max_val:
                    # Score baseado em quão razoável é o valor
                    scores[field] = 0.8 if min_val < value < max_val else 0.6
                else:
                    scores[field] = 0.3  # Valor fora do range esperado
        
        # Campos de texto
        for field in ['location_details', 'description', 'summary']:
            if field in data and data[field]:
                text_len = len(str(data[field]))
                if 10 <= text_len <= 1000:
                    scores[field] = 0.8
                else:
                    scores[field] = 0.5
        
        return scores
    
    def _validate_and_clean_data(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Valida e limpa os dados extraídos"""
        cleaned = {}
        
        for key, value in data.items():
            if value is None or (isinstance(value, str) and not value.strip()):
                continue
            
            # Limpar strings
            if isinstance(value, str):
                value = value.strip()
                # Remover múltiplos espaços
                value = re.sub(r'\s+', ' ', value)
                # Limitar tamanho
                if len(value) > 5000:
                    value = value[:5000] + '...'
            
            # Validar números
            elif isinstance(value, (int, float)):
                # Remover valores claramente incorretos
                if value < 0 and key not in ['latitude', 'longitude']:
                    continue
                
                # Arredondar floats para precisão razoável
                if isinstance(value, float) and key not in ['latitude', 'longitude']:
                    value = round(value, 2)
            
            # Validar listas
            elif isinstance(value, list):
                # Remover itens vazios
                value = [item for item in value if item and str(item).strip()]
                if not value:  # Lista vazia após limpeza
                    continue
            
            cleaned[key] = value
        
        return cleaned
    
    def extract_batch(self, urls: List[str], max_workers: int = 5) -> Dict[str, ExtractionReport]:
        """Extrai dados de múltiplas URLs em paralelo"""
        results = {}
        
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_url = {executor.submit(self.extract_from_url, url): url for url in urls}
            
            for future in as_completed(future_to_url):
                url = future_to_url[future]
                try:
                    results[url] = future.result()
                except Exception as e:
                    self.logger.error(f"Erro ao processar {url}: {e}")
                    results[url] = ExtractionReport(
                        url=url,
                        success=False,
                        errors=[str(e)]
                    )
        
        return results
