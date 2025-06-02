#!/bin/bash
# Script para executar available_apis.py com dependências vendorizadas

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Verificar se o diretório vendor existe
if [ ! -d "vendor" ]; then
    echo "❌ Erro: Diretório 'vendor' não encontrado!"
    echo "  Execute: pip download -d vendor aiohttp python-dotenv"
    exit 1
fi

# Configurar PYTHONPATH para incluir vendor
export PYTHONPATH="$SCRIPT_DIR/vendor:$PYTHONPATH"

# Executar o script
python3 -m available_apis