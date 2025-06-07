#!/usr/bin/env python3
"""
Script de teste para info_compile.py
Testa a funcionalidade básica sem precisar da API
"""

import os
import sys
from pathlib import Path

# Adicionar o diretório ao path
sys.path.insert(0, str(Path(__file__).parent))

from info_compile import GPTURLAnalyzer

def test_basic_functionality():
    """Testa funcionalidades básicas sem API"""
    
    print("=== TESTE 1: Verificação de API Key ===")
    api_key = os.getenv('OPENAI_API_KEY')
    if api_key:
        print(f"✓ API Key encontrada: {api_key[:10]}...")
    else:
        print("❌ API Key não encontrada")
        print("\nPara definir a API key:")
        print("export OPENAI_API_KEY='sk-...'")
        return False
    
    print("\n=== TESTE 2: Criação do Analisador ===")
    try:
        analyzer = GPTURLAnalyzer(api_key, max_images=5)
        print("✓ Analisador criado com sucesso")
    except Exception as e:
        print(f"❌ Erro ao criar analisador: {e}")
        return False
    
    print("\n=== TESTE 3: Extração de Conteúdo (sem API) ===")
    test_url = "https://example.com"
    try:
        text, images = analyzer.fetch_url_content(test_url)
        print(f"✓ Conteúdo extraído:")
        print(f"  - Texto: {len(text)} caracteres")
        print(f"  - Imagens encontradas: {len(images)}")
        for img in images:
            print(f"    • {img['type']}: {img.get('url', 'embedded')[:50]}...")
    except Exception as e:
        print(f"❌ Erro ao extrair conteúdo: {e}")
    
    return True

def test_disaster_sites():
    """Testa com sites relacionados a desastres"""
    
    print("\n=== TESTE 4: Sites de Desastres ===")
    
    # Sites relevantes para o Predicta
    test_sites = [
        "https://www.gdacs.org/",
        "https://firms.modaps.eosdis.nasa.gov/",
        "https://reliefweb.int/disasters"
    ]
    
    api_key = os.getenv('OPENAI_API_KEY')
    if not api_key:
        print("⚠️  Pulando teste com API - defina OPENAI_API_KEY")
        return
    
    analyzer = GPTURLAnalyzer(api_key, max_images=3)
    
    for url in test_sites[:1]:  # Testar apenas o primeiro para economizar
        print(f"\nTestando: {url}")
        try:
            text, images = analyzer.fetch_url_content(url)
            print(f"✓ Extraído: {len(text)} chars, {len(images)} imagens")
            
            # Se quiser testar a análise completa (usa tokens da API):
            # df = analyzer.analyze_url(url)
            # print(f"✓ Análise completa: {len(df)} registros")
            
        except Exception as e:
            print(f"❌ Erro: {e}")

def create_env_example():
    """Cria arquivo .env de exemplo"""
    env_example = """# Configuração do Predicta - info_compile
OPENAI_API_KEY=sk-sua-chave-aqui

# Opcional: outras APIs do projeto
# OPEN_WEATHER_APIKEY=
# NASA_FIRMS_KEY=
"""
    
    env_path = Path('.env.example')
    if not env_path.exists():
        env_path.write_text(env_example)
        print(f"\n✓ Criado arquivo {env_path}")
        print("  Copie para .env e adicione sua chave:")
        print("  cp .env.example .env")

if __name__ == "__main__":
    print("🔍 Testando info_compile.py\n")
    
    # Criar .env.example se não existir
    create_env_example()
    
    # Executar testes
    if test_basic_functionality():
        test_disaster_sites()
    
    print("\n✅ Testes concluídos!")