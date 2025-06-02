# Execução com Dependências Vendorizadas

Este projeto usa um sistema de vendoring similar ao Go, onde todas as dependências são incluídas no repositório.

## Estrutura

```
predicta/
├── available_apis.py      # Script principal
├── vendor/               # Dependências (wheels Python)
│   ├── aiohttp-*.whl
│   ├── python_dotenv-*.whl
│   └── ...
├── run.sh               # Script para execução em Linux/Mac
└── .env.example         # Exemplo de configuração
```

## Como Executar

### Opção 1: Usando o script shell (Linux/Mac)
```bash
./run.sh
```

### Opção 2: Configurando PYTHONPATH manualmente
```bash
export PYTHONPATH="./vendor:$PYTHONPATH"
python3 available_apis.py
```

### Opção 3: Instalando em um venv isolado
```bash
# Criar ambiente virtual
python3 -m venv temp_env
source temp_env/bin/activate  # Linux/Mac
# ou
temp_env\Scripts\activate  # Windows

# Instalar do vendor
pip install --no-index --find-links vendor vendor/*.whl

# Executar
python available_apis.py
```

## Configuração

Crie um arquivo `.env` baseado no exemplo:
```bash
cp .env.example .env
# Edite o arquivo com suas chaves de API
```

## Atualizando Dependências

Para atualizar as dependências vendorizadas:

```bash
# Em um ambiente virtual limpo
pip install aiohttp python-dotenv
pip download -d vendor -r <(pip freeze) --no-deps
```

## Vantagens do Vendoring

1. **Reprodutibilidade**: Mesmas versões exatas em qualquer máquina
2. **Sem instalação**: Não precisa de pip install
3. **Offline**: Funciona sem conexão com internet
4. **Isolamento**: Não polui o ambiente Python global

## Observações

- Os arquivos `.whl` no diretório vendor são específicos para a plataforma
- Para suportar múltiplas plataformas, inclua wheels para cada uma
- O arquivo `.env` não deve ser commitado (contém chaves privadas)