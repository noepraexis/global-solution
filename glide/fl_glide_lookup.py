#!/usr/bin/env python3
"""
Script para obter todos os GLIDE numbers de enchentes (FL) do Brasil
e enumerar informações de desastres na internet
GLIDE: Global Disaster Identifier
"""

import requests
import json
from datetime import datetime
import csv
import time
import logging
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Any
from urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter

# Importar o serviço de busca de incidentes
from find_incident import IncidentSearchService, SearchResult


# Data Models
@dataclass
class DisasterInfo:
    """Informação completa de um desastre"""
    glide: str
    name: str
    date: str
    type: str
    status: str
    country: str = "Brazil"
    internet_sources: List[SearchResult] = field(default_factory=list)
    total_online_references: int = 0
    
    def to_dict(self) -> Dict[str, Any]:
        """Converte para dicionário serializável"""
        data = asdict(self)
        # Converter SearchResult objects para dicts
        data['internet_sources'] = [
            {
                'title': source.title,
                'link': source.link,
                'snippet': source.snippet,
                'display_link': source.display_link
            }
            for source in self.internet_sources
        ]
        return data


@dataclass
class DisasterCollection:
    """Coleção de desastres com metadados"""
    disasters: List[DisasterInfo] = field(default_factory=list)
    generated_at: str = field(default_factory=lambda: datetime.now().isoformat())
    total_count: int = 0
    
    def add_disaster(self, disaster: DisasterInfo):
        """Adiciona um desastre à coleção"""
        self.disasters.append(disaster)
        self.total_count = len(self.disasters)
    
    def to_dict(self) -> Dict[str, Any]:
        """Converte para dicionário serializável"""
        return {
            'generated_at': self.generated_at,
            'total_count': self.total_count,
            'disasters': [d.to_dict() for d in self.disasters]
        }


class GLIDELookup:
    def __init__(self):
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
        
    def fetch_brazil_floods(self) -> DisasterCollection:
        """Busca todos os desastres de enchente do Brasil"""
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
    
    def enrich_with_internet_data(self, collection: DisasterCollection, max_sources: int = 5):
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
    
    def save_to_json(self, collection: DisasterCollection, filename="brazil_floods_glide_enriched.json"):
        """Salva os resultados em formato JSON"""
        with open(filename, "w", encoding="utf-8") as f:
            json.dump(collection.to_dict(), f, ensure_ascii=False, indent=2)
        self.logger.info(f"Dados salvos em {filename}")
    
    def save_to_csv(self, collection: DisasterCollection, filename="brazil_floods_glide_enriched.csv"):
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
    
    def print_summary(self, collection: DisasterCollection):
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
    
    def save_detailed_report(self, collection: DisasterCollection, filename="brazil_floods_detailed_report.txt"):
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

def main():
    print("Iniciando busca por GLIDE numbers de enchentes no Brasil...")
    
    lookup = GLIDELookup()
    
    # Buscar desastres no ReliefWeb
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

if __name__ == "__main__":
    main()
