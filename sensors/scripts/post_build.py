#!/usr/bin/env python3
"""
Post-build script para ESP32 que verifica e corrige problemas após a compilação.

Este script é executado automaticamente pelo PlatformIO após o processo de
compilação e realiza as seguintes tarefas:
1. Verifica se a compilação foi bem-sucedida examinando arquivos gerados
2. Realiza limpeza de arquivos temporários e caches
3. Gera relatório detalhado de utilização de memória
4. Identifica possíveis problemas de recursos limitados
5. Provê feedback visual sobre o status da compilação

O script é projetado seguindo princípios SOLID, especialmente responsabilidade
única, com funções modulares que têm propósitos bem definidos.

Autor: Leonardo Sena (slayerlab)
Versão: 2.0.0
Data: 11/05/2025
"""

import os
import sys
import glob
import subprocess
from typing import List, Dict, Optional, Any


# Configurações e constantes globais
# Este script é executado pelo PlatformIO, que injeta a variável 'env'
# O PlatformIO possui um método próprio de processamento de scripts Python
# Quando este script é executado diretamente pelo PlatformIO, a variável 'env'
# já está disponível no escopo global

# Configurações padrão caso seja executado fora do contexto do PlatformIO
if 'env' not in globals():
    print("AVISO: Este script foi projetado para ser executado pelo PlatformIO")
    print("Definindo variáveis padrão para execução standalone")

    class DummyEnv(dict):
        def __init__(self):
            self['BUILD_DIR'] = os.path.join(os.getcwd(), '.pio/build/esp32dev')
            self['PROJECT_DIR'] = os.getcwd()
            self['PIOENV'] = 'esp32dev'

    env = DummyEnv()

# Extrai variáveis do ambiente
BUILD_DIR = env['BUILD_DIR']
PROJECT_DIR = env['PROJECT_DIR']
ENV_NAME = env['PIOENV']

# Constantes para análise de memória
FLASH_TOTAL = 1310720  # 1.25MB para ESP32
RAM_TOTAL = 327680     # 320KB para ESP32
FLASH_WARNING_THRESHOLD = 90  # Percentual
RAM_WARNING_THRESHOLD = 80    # Percentual


def verify_build_success() -> bool:
    """
    Verifica se a compilação foi bem-sucedida e exibe informações.

    Examina a existência e o tamanho do arquivo firmware.bin para
    determinar se a compilação foi concluída com sucesso. Exibe
    informações de diagnóstico sobre o firmware gerado.

    Returns:
        bool: True se a compilação foi bem-sucedida, False caso contrário
    """
    print("\n============== PÓS-PROCESSAMENTO DA COMPILAÇÃO ==============")

    # Verifica se o firmware foi gerado
    firmware_path = os.path.join(BUILD_DIR, "firmware.bin")

    if os.path.exists(firmware_path):
        firmware_size = os.path.getsize(firmware_path) / 1024.0  # KB
        print(f"✓ Compilação bem-sucedida!")
        print(f"  Firmware gerado: {firmware_path}")
        print(f"  Tamanho: {firmware_size:.2f} KB")

        # Exibe informação adicional se a compilação for para performance
        if ENV_NAME == "esp32dev_performance":
            print(f"  Ambiente de performance otimizado utilizado")
        return True
    else:
        print("✗ Compilação falhou ou firmware não encontrado!")
        return False


def clean_temp_files() -> None:
    """
    Limpa arquivos temporários criados durante o build.

    Remove arquivos de dependência (.d) e outros arquivos temporários
    que não são necessários após a compilação, para liberar espaço
    e evitar conflitos em compilações futuras.
    """
    print("✓ Realizando limpeza de arquivos temporários")

    # Lista de padrões de arquivos temporários a remover
    temp_patterns = [
        os.path.join(BUILD_DIR, "**", "*.d"),
        os.path.join(BUILD_DIR, "**", ".directory"),
        os.path.join(BUILD_DIR, "**", "*.o.tmp"),
    ]

    # Remove arquivos temporários
    files_removed = 0
    for pattern in temp_patterns:
        for file_path in glob.glob(pattern, recursive=True):
            try:
                os.remove(file_path)
                files_removed += 1
            except OSError as e:
                print(f"  Aviso: Não foi possível remover {file_path}: {e}")

    if files_removed > 0:
        print(f"  {files_removed} arquivos temporários removidos")
    else:
        print("  Nenhum arquivo temporário encontrado para remoção")


def find_size_tool() -> Optional[str]:
    """
    Localiza a ferramenta apropriada para análise de tamanho do firmware.

    Procura em locais padrão e alternativos pela ferramenta xtensa-esp32-elf-size
    que é usada para analisar o tamanho das seções do firmware.

    Returns:
        Optional[str]: Caminho para a ferramenta de análise encontrada,
                      ou None se nenhuma ferramenta válida for encontrada
    """
    # Lista de possíveis executáveis da ferramenta size para ESP32
    size_tools = [
        "xtensa-esp32-elf-size",              # Nome padrão do PlatformIO
        "xtensa-esp-elf-size",                # Alternativa em alguns sistemas
        os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size"),
        # Caminhos adicionais em diferentes sistemas operacionais
        os.path.join(PROJECT_DIR, ".pio", "packages", "toolchain-xtensa-esp32", "bin", "xtensa-esp32-elf-size")
    ]

    # Testa cada ferramenta até encontrar uma que funcione
    for tool in size_tools:
        try:
            result = subprocess.run(
                [tool, "--version"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=2
            )
            if result.returncode == 0:
                return tool
        except (subprocess.SubprocessError, FileNotFoundError, OSError):
            continue

    return None


def parse_size_output(stdout: str) -> Dict[str, int]:
    """
    Analisa a saída do comando size para extrair informações de memória.

    Extrai os valores de text, data e bss da saída do comando xtensa-esp32-elf-size,
    que são usados para calcular o uso de flash e RAM.

    Args:
        stdout (str): A saída do comando size

    Returns:
        Dict[str, int]: Dicionário com os valores de text, data e bss
    """
    try:
        # Obtém a linha que contém os números (geralmente a segunda linha)
        lines = stdout.strip().split('\n')
        if len(lines) < 2:
            return {"text": 0, "data": 0, "bss": 0}

        # Extrai os valores numéricos (text, data, bss)
        parts = lines[1].split()
        if len(parts) < 3:
            return {"text": 0, "data": 0, "bss": 0}

        return {
            "text": int(parts[0]),
            "data": int(parts[1]),
            "bss": int(parts[2])
        }
    except (ValueError, IndexError) as e:
        print(f"  Aviso: Erro ao analisar saída do size: {e}")
        return {"text": 0, "data": 0, "bss": 0}


def run_size_command(size_tool: str, elf_path: str, args: List[str] = None) -> Optional[str]:
    """
    Executa o comando size com os argumentos especificados.

    Encapsula a execução do comando size em um método separado para facilitar
    o tratamento de erros e padronizar a interface.

    Args:
        size_tool (str): Caminho para a ferramenta size
        elf_path (str): Caminho para o arquivo ELF do firmware
        args (List[str], optional): Argumentos adicionais para o comando size

    Returns:
        Optional[str]: Saída do comando ou None em caso de erro
    """
    if args is None:
        args = []

    try:
        cmd = [size_tool] + args + [elf_path]
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=5
        )

        if result.returncode == 0:
            return result.stdout
        else:
            print(f"  Aviso: Comando size falhou com código {result.returncode}")
            print(f"  Erro: {result.stderr}")
            return None
    except subprocess.SubprocessError as e:
        print(f"  Aviso: Erro ao executar comando size: {e}")
        return None


def calculate_resource_usage(size_data: Dict[str, int]) -> Dict[str, Any]:
    """
    Calcula a utilização de recursos com base nos dados de tamanho.

    Calcula o uso de flash e RAM, além de suas porcentagens em relação
    ao total disponível, com base nos dados obtidos do comando size.

    Args:
        size_data (Dict[str, int]): Dados de tamanho obtidos com parse_size_output

    Returns:
        Dict[str, Any]: Dados de utilização de recursos calculados
    """
    text = size_data.get("text", 0)
    data = size_data.get("data", 0)
    bss = size_data.get("bss", 0)

    flash_used = text + data
    ram_used = data + bss

    flash_percent = (flash_used / FLASH_TOTAL) * 100
    ram_percent = (ram_used / RAM_TOTAL) * 100

    return {
        "flash_used": flash_used,
        "ram_used": ram_used,
        "flash_percent": flash_percent,
        "ram_percent": ram_percent,
        "flash_warning": flash_percent > FLASH_WARNING_THRESHOLD,
        "ram_warning": ram_percent > RAM_WARNING_THRESHOLD
    }


def print_resource_usage(usage_data: Dict[str, Any]) -> None:
    """
    Imprime informações de utilização de recursos de forma formatada.

    Exibe o uso de flash e RAM, incluindo valores absolutos e percentuais,
    com avisos caso os limites estejam próximos.

    Args:
        usage_data (Dict[str, Any]): Dados de utilização de recursos
    """
    print(f"\n  Utilização de recursos:")
    print(f"  Flash: {usage_data['flash_used']/1024:.1f}KB de {FLASH_TOTAL/1024:.0f}KB "
          f"({usage_data['flash_percent']:.1f}%)")
    print(f"  RAM:   {usage_data['ram_used']/1024:.1f}KB de {RAM_TOTAL/1024:.0f}KB "
          f"({usage_data['ram_percent']:.1f}%)")

    # Exibe avisos se próximo dos limites
    if usage_data['flash_warning']:
        print("  ⚠️ ATENÇÃO: Uso de Flash próximo ao limite!")
    if usage_data['ram_warning']:
        print("  ⚠️ ATENÇÃO: Uso de RAM próximo ao limite!")


def generate_memory_report() -> None:
    """
    Gera relatório de utilização de memória para o firmware.

    Utiliza a ferramenta size para analisar o arquivo ELF do firmware
    e gerar um relatório detalhado sobre o uso de memória, incluindo
    tamanho de cada seção e utilização total de flash e RAM.
    """
    print("✓ Gerando relatório de memória")

    elf_path = os.path.join(BUILD_DIR, "firmware.elf")
    if not os.path.exists(elf_path):
        print("  Arquivo ELF não encontrado")
        return

    # Localiza a ferramenta size apropriada
    size_tool = find_size_tool()
    if not size_tool:
        print("  Ferramenta de análise de tamanho não encontrada")
        return

    # Gera relatório detalhado por seção
    detailed_output = run_size_command(size_tool, elf_path, ["-A"])
    if detailed_output:
        print("  Análise detalhada por seção:")
        lines = detailed_output.strip().split('\n')
        # Imprime cabeçalho e primeiras linhas relevantes
        for line in lines[:min(6, len(lines))]:
            print(f"  {line}")

    # Gera resumo total
    summary_output = run_size_command(size_tool, elf_path)
    if summary_output:
        print("\n  Resumo de memória:")
        lines = summary_output.strip().split('\n')
        if len(lines) >= 2:
            print(f"  {lines[0]}")
            print(f"  {lines[1]}")

            # Analisa os resultados e calcula utilização
            size_data = parse_size_output(summary_output)
            usage_data = calculate_resource_usage(size_data)
            print_resource_usage(usage_data)

            # Diagnóstico adicional
            if usage_data['flash_percent'] > 95:
                print("\n  ⚠️ CRÍTICO: O firmware está ocupando quase todo o espaço disponível!")
                print("     Considere otimizar o código ou remover funcionalidades.")


def main() -> int:
    """
    Função principal do script de pós-compilação.

    Coordena a execução de todas as tarefas de pós-processamento
    e garante que elas sejam executadas na ordem correta.

    Returns:
        int: Código de saída (0 para sucesso, 1 para falha)
    """
    try:
        success = verify_build_success()

        if success:
            clean_temp_files()
            generate_memory_report()
            print("✓ Processamento pós-compilação concluído com sucesso")
            exit_code = 0
        else:
            print("✗ Não foi possível realizar processamento pós-compilação")
            exit_code = 1

        print("================================================================\n")
        return exit_code

    except Exception as e:
        print(f"✗ Erro fatal durante o pós-processamento: {e}")
        print("================================================================\n")
        return 1


# Ponto de entrada do script
if __name__ == "__main__":
    sys.exit(main())
else:
    # Quando importado pelo PlatformIO
    main()