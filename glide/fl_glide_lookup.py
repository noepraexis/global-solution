#!/usr/bin/env python3
"""
Script modular para análise preditiva de desastres naturais
Versão 0.1.0 - Arquitetura extensível com módulos especializados
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
from pathlib import Path
import importlib
import sys

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
    precipitation_mm: Optional[float] = None
    temperature_c: Optional[float] = None
    humidity_percent: Optional[float] = None
    wind_speed_kmh: Optional[float] = None
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
        self.analysis_module = self._load_module(module_name)
        
        # Extrator de dados web - apenas se disponível
        try:
            from data_extractors import WebDataExtractor
            self.web_extractor = WebDataExtractor()
        except ImportError:
            self.logger.warning("Módulo data_extractors não encontrado. Extração web desabilitada.")
            self.web_extractor = None
        
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
    
    def _load_module(self, module_name: str):
        """Carrega módulo de análise específico"""
        try:
            module = importlib.import_module(f"{module_name}_analysis")
            return module.create_analyzer()
        except ImportError:
            self.logger.warning(f"Módulo {module_name}_analysis não encontrado, usando padrão")
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
                            web_data = self.web_extractor.extract_from_url(source.link)
                            enhanced.raw_web_data[source.display_link] = web_data
                            time.sleep(0.5)  # Rate limiting
                        except Exception as e:
                            enhanced.extraction_errors.append(f"Erro ao extrair {source.link}: {str(e)}")
                
                # Criar features usando módulo específico
                if self.analysis_module and enhanced.raw_web_data:
                    from dataclasses import asdict
                    features_dict = self.analysis_module.extract_features(
                        disaster_info={
                            'glide': basic_disaster.glide,
                            'name': basic_disaster.name,
                            'date': basic_disaster.date,
                            'type': basic_disaster.type,
                            'status': basic_disaster.status,
                            'country': basic_disaster.country
                        },
                        web_content=enhanced.raw_web_data
                    )
                    
                    # Converter para DisasterFeatures
                    enhanced.features = self._create_features_from_dict(features_dict, basic_disaster)
                elif not self.analysis_module:
                    # Criar features básicas sem módulo
                    enhanced.features = self._create_basic_features(basic_disaster)
                
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
        # JSON com estrutura completa
        with open(f"{base_filename}_complete.json", "w", encoding="utf-8") as f:
            json.dump({
                'metadata': dataset.metadata,
                'features': dataset.features_matrix,
                'disasters': [
                    {
                        'glide': d.glide,
                        'name': d.name,
                        'completeness': d.data_completeness,
                        'errors': d.extraction_errors
                    }
                    for d in dataset.disasters
                ]
            }, f, ensure_ascii=False, indent=2)
        
        # CSV para features numéricas
        if dataset.features_matrix:
            import pandas as pd
            df = pd.DataFrame(dataset.features_matrix)
            df.to_csv(f"{base_filename}_features.csv", index=False)
            self.logger.info(f"Dataset ML salvo: {len(df)} amostras com {len(df.columns)} features")
        
        # Relatório de qualidade
        with open(f"{base_filename}_quality_report.txt", "w", encoding="utf-8") as f:
            f.write("RELATÓRIO DE QUALIDADE DO DATASET\n")
            f.write("="*60 + "\n\n")
            f.write(f"Total de desastres processados: {len(dataset.disasters)}\n")
            f.write(f"Amostras adequadas para ML (>60% completude): {len(dataset.features_matrix)}\n")
            f.write(f"Taxa de aproveitamento: {len(dataset.features_matrix)/len(dataset.disasters)*100:.1f}%\n\n")
            
            # Estatísticas de completude
            completeness_scores = [d.data_completeness for d in dataset.disasters]
            f.write(f"Completude média: {sum(completeness_scores)/len(completeness_scores)*100:.1f}%\n")
            f.write(f"Completude mínima: {min(completeness_scores)*100:.1f}%\n")
            f.write(f"Completude máxima: {max(completeness_scores)*100:.1f}%\n\n")
            
            # Erros mais comuns
            all_errors = []
            for d in dataset.disasters:
                all_errors.extend(d.extraction_errors)
            
            if all_errors:
                f.write("ERROS DE EXTRAÇÃO MAIS COMUNS:\n")
                from collections import Counter
                error_counts = Counter(all_errors)
                for error, count in error_counts.most_common(10):
                    f.write(f"  - {error[:100]}... ({count} ocorrências)\n")

def main():
    print("Sistema Modular de Análise Preditiva de Desastres Naturais v2.0")
    print("="*60)
    
    # Opções de execução
    print("\nEscolha o modo de operação:")
    print("1. Análise básica (coleta e busca online)")
    print("2. Análise avançada com ML (requer módulos adicionais)")
    
    modo = input("\nModo (1 ou 2) [padrão: 1]: ").strip() or "1"
    
    if modo == "1":
        # Modo básico
        lookup = ModularGLIDELookup("flood")
        collection = lookup.fetch_brazil_floods()
        
        if collection.disasters:
            # Enriquecer com dados da internet
            enrich = input("\nDeseja buscar informações na internet para cada desastre? (s/n): ").lower() == 's'
            
            if enrich:
                max_sources = int(input("Quantas fontes por desastre (padrão 5): ") or "5")
                lookup.enrich_with_internet_data(collection, max_sources)
            
            # Salvar resultados
            lookup.print_summary(collection)
            lookup.save_to_json(collection)
            lookup.save_to_csv(collection)
            lookup.save_detailed_report(collection)
            
            # Extrair apenas os GLIDE numbers
            glide_numbers = [d.glide for d in collection.disasters if d.glide]
            
            # Salvar lista simples de GLIDE numbers
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
