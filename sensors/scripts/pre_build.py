"""
Pre-build script para ESP32.

Este script resolve problemas de concorrência durante a compilação:
1. Cria estrutura de diretórios necessária
2. Limpa caches do SCons que possam estar corrompidos
3. Otimiza flags de compilação
4. Remove bibliotecas incompatíveis com ESP32

Autor: Leonardo Sena (slayerlab)
Data: 11/05/2025
Versão: 2.5.0
"""

import os
import glob
import shutil
import stat
import time

# Configurações e constantes
# Este script é executado pelo PlatformIO, que possui um mecanismo próprio
# para injetar variáveis em scripts Python durante o build

# Configurações padrão caso seja executado fora do contexto do PlatformIO
if 'env' not in globals():
    print("AVISO: Este script foi projetado para ser executado pelo PlatformIO")
    print("Executando com configurações padrão para ambiente de desenvolvimento")

    class DummyEnv(dict):
        def __init__(self):
            self['BUILD_DIR'] = os.path.join(os.getcwd(), '.pio/build/esp32dev')
            self['PROJECT_DIR'] = os.getcwd()
            self['PIOENV'] = 'esp32dev'

        def get(self, key, default=None):
            return self[key] if key in self else default

        def Append(self, **kwargs):
            pass

        def Replace(self, **kwargs):
            pass

        def Builder(self, **kwargs):
            return None

    env = DummyEnv()

# Obtém as configurações do ambiente
BUILD_DIR = env['BUILD_DIR']
PROJECT_DIR = env['PROJECT_DIR']
ENV_NAME = env['PIOENV']

# PLATFORM_DIR não está disponível em todos os ambientes
try:
    PLATFORM_DIR = env['PLATFORM_DIR']
except KeyError:
    PLATFORM_DIR = os.path.join(os.path.expanduser("~"), ".platformio", "platforms", "espressif32")

print(f"✓ Iniciando pré-processamento para ambiente {ENV_NAME}")


def ensure_directory_permissions(directory):
    """
    Garante permissões adequadas no diretório.

    O diretório terá permissões 0o777 (rwxrwxrwx), garantindo acesso
    completo para evitar problemas de concorrência.

    Args:
        directory: Caminho do diretório a modificar
    """
    if os.path.exists(directory):
        try:
            # Define permissões 0o777 (rwxrwxrwx) para garantir acesso total
            os.chmod(directory, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
            for root, dirs, files in os.walk(directory):
                # Aplica permissões para todos os subdiretórios
                for d in dirs:
                    os.chmod(os.path.join(root, d), stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)

                # Aplica permissões para todos os arquivos
                for f in files:
                    try:
                        os.chmod(os.path.join(root, f), stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
                    except Exception:
                        # Ignora arquivos que não podem ter permissões alteradas
                        pass
        except Exception as e:
            print(f"  Aviso: Erro ao definir permissões: {e}")


def create_dependency_file(src_file, build_dir):
    """
    Cria arquivo de dependência (.d) para um arquivo fonte.

    Os arquivos .d são utilizados pelo compilador para rastrear dependências.
    Criá-los antecipadamente previne erros comuns do SCons.

    Args:
        src_file: Caminho do arquivo fonte (.cpp)
        build_dir: Diretório de build

    Returns:
        bool: True se criado com sucesso, False caso contrário
    """
    base_name = os.path.basename(src_file)
    dep_file = os.path.join(build_dir, "src", f"{base_name}.d")

    try:
        # Cria arquivo de dependência vazio com permissões adequadas
        with open(dep_file, 'w') as f:
            # Adiciona uma regra de dependência básica para o arquivo
            target = os.path.join(build_dir, "src", os.path.splitext(base_name)[0] + ".o")
            f.write(f"{target}: {src_file}\n")

        # Garante permissões adequadas (0o666)
        os.chmod(dep_file, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP |
                          stat.S_IWGRP | stat.S_IROTH | stat.S_IWOTH)
        return True
    except Exception as e:
        print(f"  Aviso: Erro ao criar arquivo {dep_file}: {e}")
        return False


def create_directory_structure():
    """
    Cria estrutura de diretórios necessária para compilação.

    Esta função prepara o ambiente de build criando os diretórios
    necessários com as permissões corretas para evitar problemas
    de concorrência durante a compilação.
    """
    print("✓ Criando estrutura de diretórios para evitar problemas de concorrência")

    # Diretórios principais necessários para compilação
    dirs_to_create = [
        os.path.join(BUILD_DIR, "src"),
        os.path.join(BUILD_DIR, "lib"),
        os.path.join(BUILD_DIR, "include"),
        os.path.join(BUILD_DIR, "FrameworkArduino"),
    ]

    # Cria diretórios e garante permissões
    for directory in dirs_to_create:
        try:
            os.makedirs(directory, exist_ok=True)
            # Garante permissões adequadas
            ensure_directory_permissions(directory)
            # Cria arquivo .directory para marcar o diretório
            open(os.path.join(directory, ".directory"), 'a').close()
        except Exception as e:
            print(f"  Aviso: Erro ao criar diretório {directory}: {e}")

    # Cria arquivos de dependência para cada arquivo fonte
    print("  Criando arquivos de dependência para todos os arquivos fonte")
    src_files = glob.glob(os.path.join(PROJECT_DIR, "src", "*.cpp"))

    success_count = 0
    for src_file in src_files:
        if create_dependency_file(src_file, BUILD_DIR):
            success_count += 1

    print(f"  {success_count}/{len(src_files)} arquivos de dependência criados com sucesso")


def remove_sconsign_file(file_path):
    """
    Remove um arquivo de cache do SCons com segurança.

    Args:
        file_path: Caminho do arquivo a remover

    Returns:
        bool: True se removido com sucesso, False caso contrário
    """
    try:
        os.remove(file_path)
        print(f"  Removido: {os.path.basename(file_path)}")
        return True
    except Exception as e:
        print(f"  Aviso: Erro ao remover {file_path}: {e}")
        return False


def clean_scons_cache():
    """
    Limpa caches do SCons que podem estar corrompidos.

    Esta função remove arquivos de cache do SCons que frequentemente
    causam problemas durante a compilação, especialmente em ambientes
    com mudanças frequentes ou permissões restritas.
    """
    print("✓ Limpando caches que podem estar causando problemas")

    # Remove arquivos de cache do SCons no diretório raiz
    cache_files = glob.glob(os.path.join(PROJECT_DIR, ".pio", ".sconsign*.dblite"))
    for cache_file in cache_files:
        remove_sconsign_file(cache_file)

    # Remove arquivos temporários deixados por compilações anteriores
    tmp_files = glob.glob(os.path.join(BUILD_DIR, ".sconsign*.tmp"))
    for tmp_file in tmp_files:
        try:
            os.remove(tmp_file)
            print(f"  Removido arquivo temporário: {os.path.basename(tmp_file)}")
        except Exception as e:
            print(f"  Aviso: Erro ao remover arquivo temporário {tmp_file}: {e}")

    # Tratamento especial para o arquivo .sconsign.dblite no diretório de build
    handle_sconsign_dblite_file()


def handle_sconsign_dblite_file():
    """
    Aplica técnica avançada para lidar com o arquivo .sconsign.dblite.

    Esta função implementa uma estratégia robusta para forçar o SCons
    a recriar seu arquivo de cache (.sconsign.dblite) com permissões
    corretas, evitando assim erros comuns de compilação.
    """
    sconsign_path = os.path.join(BUILD_DIR, ".sconsign.dblite")

    try:
        # Se o arquivo existir, tenta tratá-lo adequadamente
        if os.path.exists(sconsign_path):
            if os.path.isdir(sconsign_path):
                # Se for um diretório (situação incomum mas possível), remove-o
                shutil.rmtree(sconsign_path)
                print(f"  Removido diretório inesperado: {sconsign_path}")
            else:
                try:
                    # Primeiro tenta dar permissões totais ao arquivo
                    os.chmod(sconsign_path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
                    # Então remove o arquivo
                    os.remove(sconsign_path)
                    print(f"  Removido: {os.path.basename(sconsign_path)}")
                except Exception as e:
                    print(f"  Aviso: Não foi possível remover {sconsign_path}: {e}")
                    # Se falhar, tenta renomear em vez de remover
                    try:
                        backup_name = f"{sconsign_path}.bak"
                        os.rename(sconsign_path, backup_name)
                        print(f"  Renomeado para: {os.path.basename(backup_name)}")
                    except Exception:
                        pass

        # Estratégia principal: criar diretório e depois removê-lo
        # para forçar o SCons a recriar o arquivo corretamente
        try:
            os.makedirs(sconsign_path, exist_ok=True)
            print(f"  Criado diretório temporário: {sconsign_path}")
            # Aguarda para garantir que o sistema de arquivos registre a operação
            time.sleep(0.1)
            # Remove o diretório, forçando o SCons a recriar o arquivo corretamente
            shutil.rmtree(sconsign_path)
            print(f"  Removido diretório temporário: {sconsign_path}")
        except Exception as e:
            print(f"  Aviso: Erro ao manipular diretório temporário: {e}")
    except Exception as e:
        print(f"  Aviso: Erro ao manipular cache: {e}")


def check_incompatible_libraries():
    """
    Verifica e remove bibliotecas incompatíveis com ESP32.

    Algumas bibliotecas são específicas para outras plataformas e causam
    conflitos quando presentes em um projeto ESP32. Esta função identifica
    e remove essas bibliotecas para prevenir erros de compilação.
    """
    print("✓ Verificando e removendo bibliotecas incompatíveis com ESP32")

    # Lista de bibliotecas incompatíveis com ESP32
    incompatible_libs = [
        "AsyncTCP_RP2040W",  # Específica para Raspberry Pi RP2040
        "ESPAsyncTCP"        # Específica para ESP8266
    ]

    libdeps_dir = os.path.join(PROJECT_DIR, ".pio", "libdeps", ENV_NAME)

    # Se o diretório de bibliotecas não existir, não há o que verificar
    if not os.path.exists(libdeps_dir):
        return

    # Verifica e remove cada biblioteca incompatível
    for lib in incompatible_libs:
        lib_path = os.path.join(libdeps_dir, lib)
        if os.path.exists(lib_path) and os.path.isdir(lib_path):
            print(f"  Removendo biblioteca incompatível: {lib}")
            try:
                shutil.rmtree(lib_path)
            except Exception as e:
                print(f"  Aviso: Não foi possível remover {lib}: {e}")


def patch_build_flags():
    """
    Modifica flags de compilação para resolver problemas específicos.

    Esta função adiciona flags que melhoram a estabilidade da compilação
    e previnem problemas comuns, especialmente relacionados a dependências
    e concorrência.
    """
    print("✓ Otimizando flags de compilação")

    # Força compilação sequencial com uma única tarefa
    if 'CONCURRENT_COMPILER_JOBS' in env:
        print("  Limitando a 1 tarefa de compilação paralela")
        env.Replace(CONCURRENT_COMPILER_JOBS=1)

    # Adiciona um builder personalizado para criar arquivos vazios
    env.Append(BUILDERS={'Touch': env.Builder(
        action=lambda target, source, env: open(target[0].path, 'a').close()
    )})

    # Adiciona flags específicas para garantir que os arquivos .d são criados corretamente
    # Isso força o GCC a gerar arquivos de dependência durante a compilação
    flags = env.get('CCFLAGS', [])
    for flag in ['-MMD', '-MP']:
        if flag not in flags:
            flags.append(flag)
    env.Replace(CCFLAGS=flags)


def main():
    """
    Executa a sequência de operações de pré-compilação.

    Esta função coordena o fluxo de operações na ordem mais eficaz
    para preparar o ambiente para compilação, evitando problemas comuns.
    """
    print("\n============== PRÉ-PROCESSAMENTO DA COMPILAÇÃO ==============")

    # A ordem é importante para maximizar eficácia e minimizar conflitos
    clean_scons_cache()            # Primeiro, limpa caches problemáticos
    check_incompatible_libraries() # Depois, remove bibliotecas incompatíveis
    create_directory_structure()   # Em seguida, cria estrutura necessária
    patch_build_flags()            # Por fim, configura flags de compilação

    # Garante permissões do diretório .pio como última operação
    ensure_directory_permissions(os.path.join(PROJECT_DIR, ".pio"))

    print("✓ Ambiente preparado com sucesso")
    print("================================================================\n")


# Ponto de entrada do script
if __name__ == "__main__":
    main()
else:
    # Execução via PlatformIO
    main()