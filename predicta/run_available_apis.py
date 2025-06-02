#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Script wrapper para executar available_apis.py com dependências vendorizadas
Similar ao conceito de 'go vendor' - não requer instalação prévia
"""

import sys
import os
import subprocess
from pathlib import Path

def setup_vendor_path():
    """Adiciona o diretório vendor ao PYTHONPATH"""
    script_dir = Path(__file__).parent
    vendor_dir = script_dir / "vendor"
    
    if not vendor_dir.exists():
        print("❌ Erro: Diretório 'vendor' não encontrado!")
        print("  Por favor, certifique-se de que o diretório vendor está presente.")
        sys.exit(1)
    
    # Adiciona vendor ao início do sys.path
    sys.path.insert(0, str(vendor_dir))
    
    # Também configura PYTHONPATH para subprocessos
    os.environ['PYTHONPATH'] = f"{vendor_dir}:{os.environ.get('PYTHONPATH', '')}"

def install_from_vendor():
    """Instala pacotes do diretório vendor em um ambiente temporário"""
    script_dir = Path(__file__).parent
    vendor_dir = script_dir / "vendor"
    
    print("🔧 Configurando ambiente isolado...")
    
    # Cria ambiente virtual temporário
    temp_venv = script_dir / ".temp_venv"
    if not temp_venv.exists():
        subprocess.run([sys.executable, "-m", "venv", str(temp_venv)], check=True)
    
    # Ativa e instalar do vendor
    if os.name == 'posix':
        pip_path = temp_venv / "bin" / "pip"
        python_path = temp_venv / "bin" / "python"
    else:
        pip_path = temp_venv / "Scripts" / "pip.exe"
        python_path = temp_venv / "Scripts" / "python.exe"
    
    # Instala todos os wheels do vendor
    wheels = list(vendor_dir.glob("*.whl"))
    if wheels:
        subprocess.run([str(pip_path), "install", "--no-index", "--find-links", str(vendor_dir)] + [str(w) for w in wheels], 
                      check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    return str(python_path)

def main():
    """Executa o script available_apis.py com as dependências vendorizadas"""
    try:
        # Tenta importa diretamente (mais rápido)
        setup_vendor_path()
        
        # Importa e executa o script principal
        import available_apis
        import asyncio
        asyncio.run(available_apis.main())
        
    except ImportError as e:
        # Se falhar, cria ambiente isolado
        print("⚠️  Criando ambiente isolado para execução...")
        python_path = install_from_vendor()
        
        # Executa o script no ambiente isolado
        script_path = Path(__file__).parent / "available_apis.py"
        result = subprocess.run([python_path, str(script_path)], env=os.environ.copy())
        sys.exit(result.returncode)

if __name__ == "__main__":
    main()
