#!/usr/bin/env python3
"""
Script modular para análise preditiva de desastres naturais
Versão 2.0 - Arquitetura extensível com módulos especializados
"""

import requests
import json
from datetime import datetime
import csv
import time
import logging
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Any, Optional, Protocol
from urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter
import importlib
import sys
from pathlib import Path

# Adicionar o diretório de módulos ao path
sys.path.insert(0, str(Path(__file__).parent / "modules"))

# Importar o serviço de busca de incidentes
from find_incident import IncidentSearchService, SearchResult


# Protocol para módulos de análise
class DisasterAnalysisModule(Protocol):
    """Interface para módulos de análise de desastres"""
    
    def extract_features(self, disaster_info: Dict[str, Any], web_content: Dict[str, Any]) -> Dict[str, Any]:
        """Extrai features específicas do tipo de desastre"""
        ...
    
    def get_required_fields(self) -> List[str]:
        """Retorna campos obrigatórios para análise"""
        ...
    
    def validate_data(self, data: Dict[str, Any]) -> bool:
        """Valida se os dados são adequados para análise"""
        ...


# Data Models Expandidos
@dataclass
class DisasterFeatures:
    """Features extraídas para análise preditiva"""
    # Campos obrigatórios primeiro
    glide: str
    disaster_type: str
    event_date: datetime
    season: str
    month: int
    year: int
    location: str
    
    # Campos opcionais depois
    days_since_last_event: Optional[int] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    region: Optional[str] = None
    state: Optional[str] = None
    precipitation_mm: Optional[float] = None
    precipitation_7d_mm: Optional[float] = None
    precipitation_30d_mm: Optional[float] = None
    temperature_c: Optional[float] = None
    temperature_max_c: Optional[float] = None
    temperature_min_c: Optional[float] = None
    humidity_percent: Optional[float] = None
    wind_speed_kmh: Optional[float] = None
    wind_max_kmh: Optional[float] = None
    atmospheric_pressure_hpa: Optional[float] = None
    affected_population: Optional[int] = None
    deaths: Optional[int] = None
    injured: Optional[int] = None
    displaced: Optional[int] = None
    economic_loss_usd: Optional[float] = None
    previous_events_30d: Optional[int] = None
    previous_events_90d: Optional[int] = None
    urbanization_index: Optional[float] = None
    vulnerability_index: Optional[float] = None
    human_development_index: Optional[float] = None
    gdp_per_capita_brl: Optional[float] = None
    population: Optional[int] = None
    elevation: Optional[float] = None
    enso_phase: Optional[str] = None
    days_in_year: Optional[int] = None
    week_of_year: Optional[int] = None
    is_weekend: Optional[bool] = None
    weather_station_distance_km: Optional[float] = None
    weather_station_name: Optional[str] = None
    news_sentiment_score: Optional[float] = None
    social_media_mentions: Optional[int] = None
    emergency_response_time_hours: Optional[float] = None
    data_quality_score: float = 0.0
    extraction_timestamp: datetime = field(default_factory=datetime.now)
    
    def to_ml_dict(self) -> Dict[str, Any]:
        """Converte para formato otimizado para ML"""
        data = asdict(self)
        # Converter datetime para timestamp
        data['event_date'] = self.event_date.timestamp() if self.event_date else None
        data['extraction_timestamp'] = self.extraction_timestamp.timestamp()
        return data


@dataclass
class EnhancedDisasterInfo:
    """Informação expandida de desastre com features para ML"""
    # Dados básicos (original)
    glide: str
    name: str
    date: str
    type: str
    status: str
    country: str = "Brazil"
    
    # Dados expandidos
    features: Optional[DisasterFeatures] = None
    raw_web_data: Dict[str, Any] = field(default_factory=dict)
    internet_sources: List[SearchResult] = field(default_factory=list)
    total_online_references: int = 0
    
    # Metadados de qualidade
    data_completeness: float = 0.0
    extraction_errors: List[str] = field(default_factory=list)
    
    def calculate_completeness(self) -> float:
        """Calcula o percentual de completude dos dados"""
        if not self.features:
            return 0.0
            
        total_fields = 0
        filled_fields = 0
        
        for field_name, field_value in asdict(self.features).items():
            if field_name not in ['extraction_timestamp', 'data_quality_score']:
                total_fields += 1
                if field_value is not None:
                    filled_fields += 1
        
        self.data_completeness = filled_fields / total_fields if total_fields > 0 else 0.0
        return self.data_completeness


@dataclass
class MLReadyDataset:
    """Dataset preparado para treinamento de ML"""
    disasters: List[EnhancedDisasterInfo] = field(default_factory=list)
    features_matrix: List[Dict[str, Any]] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def prepare_for_training(self):
        """Prepara o dataset para treinamento"""
        self.features_matrix = []
        
        for disaster in self.disasters:
            if disaster.features and disaster.data_completeness > 0.6:  # Mínimo 60% de completude
                self.features_matrix.append(disaster.features.to_ml_dict())
        
        self.metadata = {
            'total_samples': len(self.features_matrix),
            'feature_names': list(self.features_matrix[0].keys()) if self.features_matrix else [],
            'generation_timestamp': datetime.now().isoformat(),
            'min_completeness_threshold': 0.6
        }


class ModularGLIDELookup:
    def __init__(self, module_name: str = "flood"):
        # URL base da API do ReliefWeb que contém dados GLIDE
        self.base_url = "https://api.reliefweb.int/v1/disasters"
        self.headers = {
            "User-Agent": "GLIDE-FL-Brazil-Lookup/1.0"
        }
        
        # Configurar logging
        self.logger = self._setup_logger()
        
        # Criar sessão com retry
        self.session = self._create_session()
        
        # Serviço de busca de incidentes
        self.incident_search = IncidentSearchService()
        
        # Carregar módulo específico
        self.analysis_module = self._load_analysis_module(module_name)
        
        # Extrator de dados web - apenas se disponível
        try:
            from data_extractors import WebDataExtractor
            self.web_extractor = WebDataExtractor()
        except ImportError:
            self.logger.warning("Módulo data_extractors não encontrado. Extração web desabilitada.")
            self.web_extractor = None
        
        # Sistema de enriquecimento de dados
        try:
            from data_enrichment import DataEnricher
            self.data_enricher = DataEnricher(
                inmet_dir="inmet",
                emdat_file="emdat/public_emdat_custom_request_2025-06-06_0e25b758-5490-41a1-a49f-fcbea70c84ee.xlsx"
            )
            self.logger.info("Sistema de enriquecimento de dados carregado com sucesso")
        except Exception as e:
            self.logger.warning(f"Sistema de enriquecimento não disponível: {e}")
            self.data_enricher = None
        
        # Cache para evitar re-extração
        self.extraction_cache = {}
    
    def _setup_logger(self) -> logging.Logger:
        """Configura o sistema de logging"""
        logger = logging.getLogger(self.__class__.__name__)
        logger.setLevel(logging.INFO)
        
        if not logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            handler.setFormatter(formatter)
            logger.addHandler(handler)
        
        return logger
    
    def _create_session(self) -> requests.Session:
        """Cria sessão com retry configurado"""
        session = requests.Session()
        
        # Configurar estratégia de retry
        retry_strategy = Retry(
            total=3,
            backoff_factor=1,
            status_forcelist=[429, 500, 502, 503, 504],
            allowed_methods=["HEAD", "GET", "OPTIONS"]
        )
        
        adapter = HTTPAdapter(max_retries=retry_strategy)
        session.mount("http://", adapter)
        session.mount("https://", adapter)
        
        return session
    
    def _load_analysis_module(self, module_name: str):
        """Carrega módulo de análise específico"""
        try:
            # Adiciona o diretório atual ao path se necessário
            current_dir = Path(__file__).parent
            if str(current_dir) not in sys.path:
                sys.path.insert(0, str(current_dir))
            
            # Tenta importar o módulo
            module_path = f"modules.{module_name}"
            module = importlib.import_module(module_path)
            
            # Verifica se tem a função create_module
            if hasattr(module, 'create_module'):
                return module.create_module()
            else:
                self.logger.error(f"Módulo {module_name} não tem função create_module")
                return None
                
        except ImportError as e:
            self.logger.warning(f"Módulo {module_name} não encontrado: {e}")
            return None
        except Exception as e:
            self.logger.error(f"Erro ao carregar módulo {module_name}: {e}")
            return None
    
    def fetch_brazil_floods(self):  # Removida anotação de tipo
        """Busca todos os desastres de enchente do Brasil"""
        from dataclasses import dataclass, field
        from typing import List
        
        @dataclass
        class DisasterInfo:
            """Informação básica de um desastre"""
            glide: str
            name: str
            date: str
            type: str
            status: str
            country: str = "Brazil"
            internet_sources: List[SearchResult] = field(default_factory=list)
            total_online_references: int = 0
        
        @dataclass
        class DisasterCollection:
            """Coleção de desastres"""
            disasters: List[DisasterInfo] = field(default_factory=list)
            generated_at: str = field(default_factory=lambda: datetime.now().isoformat())
            total_count: int = 0
            
            def add_disaster(self, disaster: DisasterInfo):
                self.disasters.append(disaster)
                self.total_count = len(self.disasters)
            
            def to_dict(self):
                return {
                    'generated_at': self.generated_at,
                    'total_count': self.total_count,
                    'disasters': [asdict(d) for d in self.disasters]
                }
        
        collection = DisasterCollection()
        offset = 0
        limit = 100
        
        while True:
            params = {
                "appname": "glide-lookup",
                "filter[field]": "country.iso3",
                "filter[value]": "BRA",
                "fields[include][]": ["glide", "name", "date", "type", "country", "status"],
                "limit": limit,
                "offset": offset
            }
            
            try:
                response = self.session.get(
                    self.base_url, 
                    params=params, 
                    headers=self.headers,
                    timeout=30
                )
                response.raise_for_status()
                data = response.json()
                
                if "data" not in data or len(data["data"]) == 0:
                    break
                    
                # Filtrar apenas enchentes (FL)
                for disaster in data["data"]:
                    if "fields" in disaster:
                        glide = disaster["fields"].get("glide", "")
                        disaster_type = disaster["fields"].get("type", [])
                        
                        # Verificar se é enchente (FL no GLIDE ou tipo Flood)
                        if (glide and "FL-" in glide) or any("flood" in t.get("name", "").lower() for t in disaster_type):
                            disaster_info = DisasterInfo(
                                glide=glide,
                                name=disaster["fields"].get("name", ""),
                                date=disaster["fields"].get("date", {}).get("created", ""),
                                type=", ".join([t.get("name", "") for t in disaster_type]),
                                status=disaster["fields"].get("status", "")
                            )
                            collection.add_disaster(disaster_info)
                
                offset += limit
                time.sleep(0.5)  # Respeitar rate limits
                
            except requests.exceptions.RequestException as e:
                self.logger.error(f"Erro ao buscar dados do ReliefWeb: {e}")
                break
                
        return collection
    
    def enrich_with_internet_data(self, collection, max_sources: int = 5):  # Removida anotação de tipo
        """Enriquece os desastres com dados da internet"""
        self.logger.info(f"Enriquecendo {len(collection.disasters)} desastres com dados da internet...")
        
        for i, disaster in enumerate(collection.disasters, 1):
            if not disaster.glide:
                continue
                
            self.logger.info(f"[{i}/{len(collection.disasters)}] Buscando fontes para {disaster.glide}")
            
            try:
                # Buscar informações na internet
                search_response = self.incident_search.search_incident(disaster.glide)
                
                # Adicionar apenas as primeiras max_sources fontes
                disaster.internet_sources = search_response.items[:max_sources]
                disaster.total_online_references = search_response.total_results
                
                self.logger.info(f"  Encontradas {disaster.total_online_references} referências online")
                
                # Respeitar rate limit
                time.sleep(1)
                
            except Exception as e:
                self.logger.warning(f"  Erro ao buscar dados online para {disaster.glide}: {e}")
                continue
    
    def save_to_json(self, collection, filename="brazil_floods_glide_enriched.json"):
        """Salva os resultados em formato JSON"""
        with open(filename, "w", encoding="utf-8") as f:
            json.dump(collection.to_dict(), f, ensure_ascii=False, indent=2)
        self.logger.info(f"Dados salvos em {filename}")
    
    def save_to_csv(self, collection, filename="brazil_floods_glide_enriched.csv"):
        """Salva os resultados em formato CSV"""
        if not collection.disasters:
            self.logger.warning("Nenhum desastre encontrado para salvar")
            return
            
        with open(filename, "w", newline="", encoding="utf-8") as f:
            fieldnames = [
                "glide", "name", "date", "type", "status", 
                "total_online_references", "first_source_title", "first_source_link"
            ]
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            
            writer.writeheader()
            for disaster in collection.disasters:
                row = {
                    "glide": disaster.glide,
                    "name": disaster.name,
                    "date": disaster.date,
                    "type": disaster.type,
                    "status": disaster.status,
                    "total_online_references": disaster.total_online_references,
                    "first_source_title": disaster.internet_sources[0].title if disaster.internet_sources else "",
                    "first_source_link": disaster.internet_sources[0].link if disaster.internet_sources else ""
                }
                writer.writerow(row)
                
        self.logger.info(f"Dados salvos em {filename}")
    
    def print_summary(self, collection):
        """Imprime um resumo dos resultados"""
        print(f"\n{'='*80}")
        print(f"GLIDE Numbers - Enchentes no Brasil (com dados da internet)")
        print(f"{'='*80}")
        print(f"Total de eventos encontrados: {collection.total_count}")
        
        if collection.disasters:
            print(f"\nPrimeiros 10 eventos:")
            print(f"{'-'*80}")
            for i, disaster in enumerate(collection.disasters[:10], 1):
                date_str = disaster.date[:10] if disaster.date else 'N/A'
                online_refs = f"{disaster.total_online_references} refs online" if disaster.total_online_references else "não pesquisado"
                print(f"{i}. {disaster.glide} - {disaster.name}")
                print(f"   Data: {date_str} | Status: {disaster.status} | {online_refs}")
                
                if disaster.internet_sources:
                    print(f"   Primeira fonte: {disaster.internet_sources[0].display_link}")
            
            if len(collection.disasters) > 10:
                print(f"\n... e mais {len(collection.disasters) - 10} eventos")
    
    def save_detailed_report(self, collection, filename="brazil_floods_detailed_report.txt"):
        """Salva um relatório detalhado em texto"""
        with open(filename, "w", encoding="utf-8") as f:
            f.write("RELATÓRIO DETALHADO - ENCHENTES NO BRASIL\n")
            f.write(f"Gerado em: {collection.generated_at}\n")
            f.write(f"Total de desastres: {collection.total_count}\n")
            f.write("="*80 + "\n\n")
            
            for i, disaster in enumerate(collection.disasters, 1):
                f.write(f"[{i}] {disaster.glide}\n")
                f.write(f"Nome: {disaster.name}\n")
                f.write(f"Data: {disaster.date}\n")
                f.write(f"Tipo: {disaster.type}\n")
                f.write(f"Status: {disaster.status}\n")
                f.write(f"Referências online: {disaster.total_online_references}\n")
                
                if disaster.internet_sources:
                    f.write("\nFontes na Internet:\n")
                    for j, source in enumerate(disaster.internet_sources, 1):
                        f.write(f"  {j}. {source.title}\n")
                        f.write(f"     URL: {source.link}\n")
                        f.write(f"     {source.snippet}\n\n")
                
                f.write("-"*80 + "\n\n")
        
        self.logger.info(f"Relatório detalhado salvo em {filename}")
    
    def fetch_and_analyze_floods(self) -> MLReadyDataset:
        """Busca e analisa enchentes com extração completa de features"""
        dataset = MLReadyDataset()
        
        # Buscar dados básicos
        basic_collection = self.fetch_brazil_floods()
        
        self.logger.info(f"Iniciando análise detalhada de {len(basic_collection.disasters)} desastres...")
        
        for i, basic_disaster in enumerate(basic_collection.disasters, 1):
            self.logger.info(f"[{i}/{len(basic_collection.disasters)}] Analisando {basic_disaster.glide or basic_disaster.name}")
            
            # Criar versão expandida
            enhanced = EnhancedDisasterInfo(
                glide=basic_disaster.glide,
                name=basic_disaster.name,
                date=basic_disaster.date,
                type=basic_disaster.type,
                status=basic_disaster.status
            )
            
            try:
                # Buscar fontes na internet
                if basic_disaster.glide:
                    search_response = self.incident_search.search_incident(basic_disaster.glide)
                    enhanced.internet_sources = search_response.items[:10]
                    enhanced.total_online_references = search_response.total_results
                
                # Extrair dados detalhados da web apenas se o extrator estiver disponível
                if self.web_extractor and enhanced.internet_sources:
                    self.logger.info(f"  Extraindo dados de {len(enhanced.internet_sources)} fontes...")
                    
                    for source in enhanced.internet_sources[:3]:  # Limitar para performance
                        try:
                            # Usar o novo método que retorna ExtractionReport
                            extraction_report = self.web_extractor.extract_from_url(source.link)
                            
                            if extraction_report.success:
                                enhanced.raw_web_data[source.display_link] = extraction_report.data
                                
                                # Log de campos extraídos com sucesso
                                extracted_fields = [k for k, v in extraction_report.data.items() 
                                                  if v is not None and k not in ['source_url', 'extraction_timestamp', 'source_domain']]
                                if extracted_fields:
                                    self.logger.info(f"    Extraído de {source.display_link}: {', '.join(extracted_fields[:5])}")
                                
                                # Adicionar avisos se houver
                                if extraction_report.errors:
                                    for error in extraction_report.errors:
                                        enhanced.extraction_errors.append(f"Aviso em {source.link}: {error}")
                            else:
                                enhanced.extraction_errors.append(f"Falha ao extrair {source.link}: {', '.join(extraction_report.errors)}")
                            
                            time.sleep(0.5)  # Rate limiting
                        except Exception as e:
                            enhanced.extraction_errors.append(f"Erro ao extrair {source.link}: {str(e)}")
                
                # Preparar dados do desastre para enriquecimento
                disaster_data = {
                    'glide': basic_disaster.glide,
                    'name': basic_disaster.name,
                    'date': basic_disaster.date,
                    'type': basic_disaster.type,
                    'status': basic_disaster.status,
                    'country': basic_disaster.country,
                    'internet_sources': enhanced.internet_sources,  # Passar URLs para extração web
                    'raw_web_data': enhanced.raw_web_data  # Passar dados web também
                }
                
                # Aplicar enriquecimento de dados se disponível
                if self.data_enricher:
                    self.logger.info("  Aplicando enriquecimento de dados...")
                    try:
                        enriched_data = self.data_enricher.enrich_disaster_data(disaster_data)
                        
                        # Mesclar com dados da web
                        for source_data in enhanced.raw_web_data.values():
                            if isinstance(source_data, dict):
                                # Priorizar dados da web quando disponíveis
                                for key, value in source_data.items():
                                    if key not in enriched_data or enriched_data[key] is None:
                                        enriched_data[key] = value
                        
                        disaster_data = enriched_data
                        
                        # Log de campos enriquecidos
                        enriched_fields = [k for k in enriched_data.keys() 
                                         if k not in ['glide', 'name', 'date', 'type', 'status', 'country']
                                         and enriched_data[k] is not None]
                        if enriched_fields:
                            self.logger.info(f"    Campos enriquecidos: {', '.join(enriched_fields[:10])}")
                        
                    except Exception as e:
                        self.logger.warning(f"    Erro no enriquecimento: {str(e)}")
                        enhanced.extraction_errors.append(f"Erro no enriquecimento: {str(e)}")
                
                # Criar features usando módulo específico
                if self.analysis_module:
                    features_dict = self.analysis_module.extract_features(
                        disaster_info=disaster_data,
                        web_content=enhanced.raw_web_data
                    )
                    
                    # Mesclar com dados enriquecidos
                    for key, value in disaster_data.items():
                        if key not in features_dict or features_dict[key] is None:
                            features_dict[key] = value
                    
                    # Converter para DisasterFeatures
                    enhanced.features = self._create_features_from_dict(features_dict, basic_disaster)
                else:
                    # Criar features básicas
                    enhanced.features = self._create_basic_features(basic_disaster)
                    
                    # Adicionar dados enriquecidos às features
                    if self.data_enricher:
                        for attr, value in disaster_data.items():
                            if hasattr(enhanced.features, attr) and value is not None:
                                setattr(enhanced.features, attr, value)
                
                # Calcular completude
                enhanced.calculate_completeness()
                
                # Adicionar ao dataset
                dataset.disasters.append(enhanced)
                
                self.logger.info(f"  Completude dos dados: {enhanced.data_completeness:.1%}")
                
            except Exception as e:
                self.logger.error(f"  Erro na análise: {str(e)}")
                enhanced.extraction_errors.append(str(e))
                dataset.disasters.append(enhanced)
            
            # Rate limiting
            time.sleep(1)
        
        # Preparar dataset para ML
        dataset.prepare_for_training()
        
        return dataset
    
    def _create_basic_features(self, basic_disaster) -> DisasterFeatures:
        """Cria features básicas quando não há módulo específico"""
        try:
            event_date = datetime.fromisoformat(basic_disaster.date.replace('+00:00', ''))
        except:
            event_date = datetime.now()
        
        return DisasterFeatures(
            glide=basic_disaster.glide or "",
            disaster_type=basic_disaster.type,
            event_date=event_date,
            season=self._get_season(event_date),
            month=event_date.month,
            year=event_date.year,
            location=basic_disaster.name.split(":")[-1].strip() if ":" in basic_disaster.name else "Brazil"
        )
    
    def _create_features_from_dict(self, features_dict: Dict[str, Any], basic_disaster) -> DisasterFeatures:
        """Cria objeto DisasterFeatures a partir do dicionário extraído"""
        # Parser de data
        try:
            event_date = datetime.fromisoformat(basic_disaster.date.replace('+00:00', ''))
        except:
            event_date = datetime.now()
        
        return DisasterFeatures(
            glide=basic_disaster.glide,
            disaster_type=basic_disaster.type,
            event_date=event_date,
            season=self._get_season(event_date),
            month=event_date.month,
            year=event_date.year,
            location=features_dict.get('location', ''),
            latitude=features_dict.get('latitude'),
            longitude=features_dict.get('longitude'),
            precipitation_mm=features_dict.get('precipitation_mm'),
            temperature_c=features_dict.get('temperature_c'),
            humidity_percent=features_dict.get('humidity_percent'),
            wind_speed_kmh=features_dict.get('wind_speed_kmh'),
            affected_population=features_dict.get('affected_population'),
            deaths=features_dict.get('deaths'),
            injured=features_dict.get('injured'),
            displaced=features_dict.get('displaced'),
            economic_loss_usd=features_dict.get('economic_loss_usd'),
            news_sentiment_score=features_dict.get('sentiment_score'),
            data_quality_score=features_dict.get('quality_score', 0.5)
        )
    
    def _get_season(self, date: datetime) -> str:
        """Determina a estação do ano (hemisfério sul)"""
        month = date.month
        if month in [12, 1, 2]:
            return "summer"
        elif month in [3, 4, 5]:
            return "autumn"
        elif month in [6, 7, 8]:
            return "winter"
        else:
            return "spring"
    
    def save_ml_dataset(self, dataset: MLReadyDataset, base_filename: str = "brazil_floods_ml"):
        """Salva dataset em formatos otimizados para ML"""
        import os
        from datetime import datetime
        
        # Criar diretório de saída se não existir
        output_dir = "ml_datasets"
        os.makedirs(output_dir, exist_ok=True)
        
        # Gerar timestamp legível para nome único
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        unique_filename = f"{base_filename}_{timestamp}"
        
        # Caminho completo dos arquivos
        json_path = os.path.join(output_dir, f"{unique_filename}_complete.json")
        csv_path = os.path.join(output_dir, f"{unique_filename}_features.csv")
        report_path = os.path.join(output_dir, f"{unique_filename}_quality_report.txt")
        
        # Criar também um link simbólico para o "latest" de cada tipo
        latest_json = os.path.join(output_dir, f"{base_filename}_latest_complete.json")
        latest_csv = os.path.join(output_dir, f"{base_filename}_latest_features.csv")
        latest_report = os.path.join(output_dir, f"{base_filename}_latest_quality_report.txt")
        
        # JSON com estrutura completa
        with open(json_path, "w", encoding="utf-8") as f:
            # Preparar dados para serialização
            export_data = {
                'metadata': dataset.metadata,
                'features': dataset.features_matrix,
                'disasters': []
            }
            
            # Adicionar informações detalhadas de cada desastre
            for d in dataset.disasters:
                disaster_data = {
                    'glide': d.glide,
                    'name': d.name,
                    'date': d.date,
                    'type': d.type,
                    'status': d.status,
                    'completeness': d.data_completeness,
                    'errors': d.extraction_errors,
                    'total_online_references': d.total_online_references,
                    'raw_features': None
                }
                
                # Adicionar features se existirem
                if d.features:
                    disaster_data['raw_features'] = d.features.to_ml_dict()
                
                export_data['disasters'].append(disaster_data)
            
            json.dump(export_data, f, ensure_ascii=False, indent=2, default=str)
            self.logger.info(f"Dataset JSON salvo em: {json_path}")
        
        # CSV para features numéricas - incluir TODOS os desastres
        all_features = []
        for d in dataset.disasters:
            if d.features:
                features_dict = d.features.to_ml_dict()
                features_dict['glide'] = d.glide
                features_dict['completeness'] = d.data_completeness
                all_features.append(features_dict)
        
        if all_features:
            try:
                import pandas as pd
                df = pd.DataFrame(all_features)
                df.to_csv(csv_path, index=False)
                self.logger.info(f"Dataset CSV salvo em: {csv_path} ({len(df)} registros)")
            except ImportError:
                self.logger.warning("Pandas não instalado. Salvando CSV manualmente...")
                # Salvar CSV manualmente
                if all_features:
                    headers = list(all_features[0].keys())
                    with open(csv_path, 'w', encoding='utf-8') as f:
                        # Escrever cabeçalhos
                        f.write(','.join(headers) + '\n')
                        # Escrever dados
                        for row in all_features:
                            values = [str(row.get(h, '')) for h in headers]
                            f.write(','.join(values) + '\n')
                    self.logger.info(f"Dataset CSV salvo em: {csv_path} ({len(all_features)} registros)")
        
        # Relatório de qualidade detalhado
        with open(report_path, "w", encoding="utf-8") as f:
            f.write("RELATÓRIO DE QUALIDADE DO DATASET - ANÁLISE DE ENCHENTES BRASIL\n")
            f.write("="*80 + "\n\n")
            f.write(f"Data de geração: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Total de desastres processados: {len(dataset.disasters)}\n")
            f.write(f"Total de features extraídas: {len(all_features)}\n")
            f.write(f"Amostras adequadas para ML (>60% completude): {len(dataset.features_matrix)}\n")
            
            if dataset.disasters:
                f.write(f"Taxa de aproveitamento: {len(dataset.features_matrix)/len(dataset.disasters)*100:.1f}%\n\n")
                
                # Estatísticas de completude
                completeness_scores = [d.data_completeness for d in dataset.disasters]
                f.write("ESTATÍSTICAS DE COMPLETUDE:\n")
                f.write(f"  - Média: {sum(completeness_scores)/len(completeness_scores)*100:.1f}%\n")
                f.write(f"  - Mínima: {min(completeness_scores)*100:.1f}%\n")
                f.write(f"  - Máxima: {max(completeness_scores)*100:.1f}%\n\n")
                
                # Desastres por nível de completude
                f.write("DISTRIBUIÇÃO POR COMPLETUDE:\n")
                high_quality = sum(1 for s in completeness_scores if s > 0.8)
                medium_quality = sum(1 for s in completeness_scores if 0.4 <= s <= 0.8)
                low_quality = sum(1 for s in completeness_scores if s < 0.4)
                f.write(f"  - Alta (>80%): {high_quality} desastres\n")
                f.write(f"  - Média (40-80%): {medium_quality} desastres\n")
                f.write(f"  - Baixa (<40%): {low_quality} desastres\n\n")
                
                # Listar desastres com melhor qualidade
                f.write("TOP 10 DESASTRES COM MELHOR QUALIDADE DE DADOS:\n")
                sorted_disasters = sorted(dataset.disasters, key=lambda x: x.data_completeness, reverse=True)[:10]
                for i, d in enumerate(sorted_disasters, 1):
                    f.write(f"  {i}. {d.glide} - {d.name} ({d.data_completeness*100:.1f}%)\n")
                
                # Erros mais comuns
                all_errors = []
                for d in dataset.disasters:
                    all_errors.extend(d.extraction_errors)
                
                if all_errors:
                    f.write("\nERROS DE EXTRAÇÃO MAIS COMUNS:\n")
                    from collections import Counter
                    error_counts = Counter(all_errors)
                    for error, count in error_counts.most_common(10):
                        f.write(f"  - {error[:100]}... ({count} ocorrências)\n")
                
                # Estatísticas de fontes online
                f.write("\nESTATÍSTICAS DE FONTES ONLINE:\n")
                total_refs = sum(d.total_online_references for d in dataset.disasters)
                avg_refs = total_refs / len(dataset.disasters) if dataset.disasters else 0
                f.write(f"  - Total de referências encontradas: {total_refs}\n")
                f.write(f"  - Média por desastre: {avg_refs:.1f}\n")
                
                # Features mais preenchidas
                if all_features:
                    f.write("\nFEATURES MAIS COMPLETAS:\n")
                    feature_counts = {}
                    for feat_dict in all_features:
                        for key, value in feat_dict.items():
                            if value is not None and key not in ['glide', 'completeness', 'extraction_timestamp']:
                                feature_counts[key] = feature_counts.get(key, 0) + 1
                    
                    sorted_features = sorted(feature_counts.items(), key=lambda x: x[1], reverse=True)
                    for feat, count in sorted_features[:10]:
                        percentage = (count / len(all_features)) * 100
                        f.write(f"  - {feat}: {count}/{len(all_features)} ({percentage:.1f}%)\n")
        
        self.logger.info(f"Relatório de qualidade salvo em: {report_path}")
        
        # Criar links simbólicos para os arquivos mais recentes (apenas em sistemas Unix-like)
        try:
            import platform
            if platform.system() != 'Windows':
                # Remover links antigos se existirem
                for link in [latest_json, latest_csv, latest_report]:
                    if os.path.islink(link):
                        os.unlink(link)
                
                # Criar novos links
                os.symlink(os.path.basename(json_path), latest_json)
                os.symlink(os.path.basename(csv_path), latest_csv)
                os.symlink(os.path.basename(report_path), latest_report)
                
                self.logger.info("Links simbólicos 'latest' criados com sucesso")
        except Exception as e:
            self.logger.debug(f"Não foi possível criar links simbólicos: {e}")
        
        # Resumo final
        print(f"\n{'='*60}")
        print("ARQUIVOS SALVOS COM SUCESSO:")
        print(f"  - Dataset completo (JSON): {json_path}")
        print(f"  - Features numéricas (CSV): {csv_path}")
        print(f"  - Relatório de qualidade: {report_path}")
        print(f"\nTimestamp: {timestamp}")
        print(f"Diretório: {os.path.abspath(output_dir)}")
        print(f"{'='*60}\n")

def main():
    """Função principal"""
    print("Sistema Modular de Análise Preditiva de Desastres Naturais v2.0")
    print("============================================================\n")
    
    # Seleção de modo
    print("Escolha o modo de operação:")
    print("1. Análise básica (coleta e busca online)")
    print("2. Análise avançada com ML (requer módulos adicionais)")
    print()
    
    mode = input("Modo (1 ou 2) [padrão: 1]: ").strip() or "1"
    
    if mode == "1":
        # Modo básico - apenas coleta e busca
        glide_lookup = ModularGLIDELookup()
        print("\nBuscando eventos de enchente no Brasil...")
        result = glide_lookup.fetch_brazil_floods()
        
        if result.disasters:
            # Mostrar resumo
            print(f"\nTotal de eventos encontrados: {result.total_count}")
            print("\nPrimeiros 5 eventos:")
            for event in result.disasters[:5]:
                print(f"- {event.glide}: {event.name}")
                print(f"  Data: {event.date}")
                print(f"  Fontes online: {event.total_online_references}")
                print()
            
            # Salvar arquivos
            # JSON completo
            with open("brazil_fl_disasters.json", "w", encoding="utf-8") as f:
                json.dump(result.to_dict(), f, ensure_ascii=False, indent=2)
            print(f"\nDados completos salvos em brazil_fl_disasters.json")
            
            # CSV resumido
            result.to_csv("brazil_fl_disasters.csv")
            print(f"Resumo em CSV salvo em brazil_fl_disasters.csv")
            
            # Lista simples de GLIDE numbers
            glide_numbers = [d.glide for d in result.disasters]
            with open("brazil_fl_glide_numbers.txt", "w") as f:
                f.write("\n".join(glide_numbers))
            print(f"\nLista de GLIDE numbers salva em brazil_fl_glide_numbers.txt")
            
        else:
            print("Nenhum evento de enchente encontrado para o Brasil")
    
    else:
        # Modo avançado
        module = input("Módulo de análise (flood/wind/heat/etc.) [padrão: flood]: ").strip() or "flood"
        
        lookup = ModularGLIDELookup(module)
        
        # Executar análise completa
        print("\nIniciando coleta e análise de dados...")
        dataset = lookup.fetch_and_analyze_floods()
        
        # Salvar resultados
        lookup.save_ml_dataset(dataset)
        
        print(f"\nAnálise concluída!")
        print(f"Total de desastres analisados: {len(dataset.disasters)}")
        print(f"Amostras prontas para ML: {len(dataset.features_matrix)}")
        print(f"Features extraídas: {len(dataset.metadata.get('feature_names', []))}")

if __name__ == "__main__":
    main()
