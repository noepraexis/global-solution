#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Script para validar disponibilidade das APIs principais do projeto Predicta
"""

import asyncio
import aiohttp
import os
import ssl
import logging
from datetime import datetime
from typing import Dict, Tuple
from pathlib import Path
from dotenv import load_dotenv

# Configurar logging para suprimir erros de SSL durante cleanup
logging.getLogger('asyncio').setLevel(logging.CRITICAL)

# Carregar variáveis de ambiente do arquivo .env
env_path = Path(__file__).parent.parent / '.env'
load_dotenv(env_path)


class APIValidator:
    """Validador de disponibilidade de APIs externas"""
    
    def __init__(self):
        self.apis_config = {
            'NOAA_GFS': {
                'url': 'https://api.weather.gov/points/39.7456,-97.0892',
                'esperado': [200],
                'descricao': 'API de previsão meteorológica do NOAA'
            },
            'GDACS': {
                'url': 'https://www.gdacs.org/gdacsapi/api/events/geteventlist',
                'esperado': [200],
                'descricao': 'API de alertas de desastres globais',
                'params': {
                    'fromdate': '2024-01-01',
                    'todate': '2024-12-31',
                    'eventtype': 'DR'
                }
            },
            'NASA_FIRMS': {
                'url': 'https://firms.modaps.eosdis.nasa.gov/api/area/csv/61f42618175332947f84279debeda0d7/VIIRS_SNPP_NRT/-50,-16,-45,-10/1/2024-01-01',
                'esperado': [200],
                'descricao': 'API de detecção de incêndios da NASA'
            },
            'OpenWeather': {
                'url': 'https://api.openweathermap.org/data/2.5/weather',
                'esperado': [200],
                'descricao': 'API de dados meteorológicos OpenWeather',
                'params': {
                    'lat': '-15.7',
                    'lon': '-47.9',
                    'appid': os.getenv('OPEN_WEATHER_APIKEY', 'DEMO_KEY')
                }
            }
        }
        
        # Configuração SSL para evitar erros de cleanup
        self.ssl_context = ssl.create_default_context()
        self.ssl_context.check_hostname = False
        self.ssl_context.verify_mode = ssl.CERT_NONE
    
    async def testar_api(self, session: aiohttp.ClientSession, nome: str, config: Dict) -> Tuple[str, int, str]:
        """Testa uma API específica"""
        try:
            params = config.get('params', {})
            async with session.get(
                config['url'], 
                params=params if params else None,
                timeout=aiohttp.ClientTimeout(total=10),
                ssl=self.ssl_context
            ) as response:
                status = response.status
                if status in config['esperado']:
                    return nome, status, 'OK'
                else:
                    return nome, status, f'Status inesperado (esperado: {config["esperado"]})'
        except asyncio.TimeoutError:
            return nome, 0, 'Timeout'
        except aiohttp.ClientError as e:
            return nome, 0, f'Erro de conexão: {type(e).__name__}'
        except Exception as e:
            return nome, 0, f'Erro: {type(e).__name__}'
    
    async def validar_todas_apis(self):
        """Valida todas as APIs configuradas"""
        print("\n🔍 Validando APIs do Projeto Predicta...\n")
        print(f"{'API':<15} {'Status':<10} {'Resultado':<50}")
        print("-" * 75)
        
        # Criar conector com configurações de SSL
        connector = aiohttp.TCPConnector(
            ssl=self.ssl_context,
            force_close=True
        )
        
        async with aiohttp.ClientSession(connector=connector) as session:
            # Executar testes em paralelo
            tarefas = [
                self.testar_api(session, nome, config)
                for nome, config in self.apis_config.items()
            ]
            
            resultados = await asyncio.gather(*tarefas)
            
            # Exibir resultados
            todas_ok = True
            for nome, status, mensagem in resultados:
                emoji = '✅' if mensagem == 'OK' else '❌'
                print(f"{nome:<15} {status:<10} {emoji} {mensagem}")
                
                if mensagem != 'OK':
                    todas_ok = False
            
            print("\n" + "-" * 75)
            
            # Resumo
            print("\n📊 Resumo:\n")
            for nome, config in self.apis_config.items():
                print(f"  • {nome}: {config['descricao']}")
            
            # Contar APIs com problemas
            apis_com_problemas = [r for r in resultados if r[2] != 'OK']
            apis_funcionando = len(resultados) - len(apis_com_problemas)
            
            if todas_ok:
                print("\n✅ Todas as APIs estão funcionando corretamente!")
            else:
                if len(apis_com_problemas) == 1:
                    print(f"\n⚠️  {len(apis_com_problemas)} API apresentou problema ({apis_funcionando} funcionando normalmente)")
                else:
                    print(f"\n⚠️  {len(apis_com_problemas)} APIs apresentaram problemas ({apis_funcionando} funcionando normalmente)")
                print("\n💡 Problemas encontrados:")
                
                # Listar problemas específicos
                for nome, status, mensagem in resultados:
                    if mensagem != 'OK':
                        print(f"  • {nome}: {mensagem} (HTTP {status})")
                        
                        # Sugestões específicas
                        if nome == 'OpenWeather' and status == 401:
                            api_key = os.getenv('OPEN_WEATHER_APIKEY', 'DEMO_KEY')
                            if api_key == 'DEMO_KEY':
                                print("    → Configure a variável OPEN_WEATHER_APIKEY no arquivo .env")
                            else:
                                print(f"    → Verifique se a API key está válida: {api_key[:8]}...")
                        elif nome == 'GDACS' and status == 404:
                            print("    → O endpoint pode ter mudado ou estar temporariamente indisponível")
            
            # Fechar o conector adequadamente
            await connector.close()


async def main():
    """Função principal"""
    validator = APIValidator()
    await validator.validar_todas_apis()


if __name__ == "__main__":
    # Executar com tratamento de exceções
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n⚠️  Validação interrompida pelo usuário")
    except Exception as e:
        print(f"\n❌ Erro durante validação: {e}")
