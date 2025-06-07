"""
GPT URL Analyzer - Análise completa de páginas web com GPT-4 Vision

Este script analisa páginas web extraindo tanto texto quanto conteúdo visual,
utilizando GPT-4 Vision para interpretação completa do conteúdo.

Características principais:
- Extração robusta de imagens sem depender de extensões de arquivo
- Suporte para múltiplos formatos: JPEG, PNG, WebP, GIF, SVG, AVIF, etc.
- Detecção inteligente via Content-Type e magic bytes
- Análise de SVGs inline e data URIs
- OCR automático de texto em imagens
- Interpretação de gráficos e visualizações de dados
- Extração de imagens de CSS, lazy loading, e meta tags

Autor: Assistant
Versão: 2.0
"""

import os
import json
import requests
import pandas as pd
from bs4 import BeautifulSoup
from openai import OpenAI
from typing import Dict, List, Optional, Tuple
import re
from urllib.parse import urlparse, urljoin
import base64
from io import BytesIO
from PIL import Image
import mimetypes
from pathlib import Path

# Tentar carregar dotenv se disponível
try:
    from dotenv import load_dotenv
    # Procurar .env no diretório atual e no diretório pai
    env_path = Path('.env')
    if not env_path.exists():
        env_path = Path('../.env')
    if env_path.exists():
        load_dotenv(env_path)
        print(f"✓ Carregado arquivo .env de: {env_path.absolute()}")
except ImportError:
    pass

class GPTURLAnalyzer:
    """
    Classe para analisar conteúdo de URLs (texto e imagens) usando GPT-4 Vision
    """
    
    def __init__(self, api_key: str, max_images: int = 10):
        """
        Inicializa o analisador com a chave da API OpenAI
        
        Args:
            api_key: Chave da API OpenAI
            max_images: Número máximo de imagens para analisar
        """
        self.client = OpenAI(api_key=api_key)
        self.max_images = max_images
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
        })
        
    def fetch_url_content(self, url: str) -> Tuple[str, List[Dict]]:
        """
        Busca o conteúdo de uma URL incluindo texto e informações de imagens
        
        Args:
            url: URL para buscar conteúdo
            
        Returns:
            Tupla com (texto extraído, lista de dicts com info de imagens)
        """
        try:
            response = self.session.get(url, timeout=30)
            response.raise_for_status()
            
            soup = BeautifulSoup(response.text, 'html.parser')
            
            # Remover scripts e estilos
            for script in soup(["script", "style"]):
                script.decompose()
            
            # Extrair texto
            text = soup.get_text()
            lines = (line.strip() for line in text.splitlines())
            chunks = (phrase.strip() for line in lines for phrase in line.split("  "))
            text = ' '.join(chunk for chunk in chunks if chunk)
            
            # Coletar informações de imagens
            image_data = []
            seen_urls = set()
            
            # 1. Tags <img> - fonte mais comum
            for img in soup.find_all('img'):
                img_info = self._extract_image_info(img, url, 'img')
                if img_info and img_info['url'] not in seen_urls:
                    seen_urls.add(img_info['url'])
                    image_data.append(img_info)
            
            # 2. Tags <picture> e <source> - imagens responsivas
            for picture in soup.find_all('picture'):
                sources = picture.find_all('source')
                for source in sources:
                    img_info = self._extract_image_info(source, url, 'picture')
                    if img_info and img_info['url'] not in seen_urls:
                        seen_urls.add(img_info['url'])
                        image_data.append(img_info)
                
                # Fallback img dentro de picture
                img = picture.find('img')
                if img:
                    img_info = self._extract_image_info(img, url, 'picture')
                    if img_info and img_info['url'] not in seen_urls:
                        seen_urls.add(img_info['url'])
                        image_data.append(img_info)
            
            # 3. SVG inline - vetores importantes
            for svg in soup.find_all('svg'):
                svg_content = str(svg)
                if len(svg_content) > 100:  # SVGs significativos
                    svg_data = {
                        'url': None,
                        'type': 'svg_inline',
                        'content': svg_content[:1000],  # Limitar tamanho
                        'alt': svg.get('aria-label', ''),
                        'context': self._get_element_context(svg)
                    }
                    image_data.append(svg_data)
            
            # 4. Tags <object> e <embed> - podem conter SVGs
            for tag in soup.find_all(['object', 'embed']):
                if 'image' in tag.get('type', '') or tag.get('data', '').endswith('.svg'):
                    img_info = self._extract_image_info(tag, url, tag.name)
                    if img_info and img_info['url'] not in seen_urls:
                        seen_urls.add(img_info['url'])
                        image_data.append(img_info)
            
            # 5. CSS background images
            elements_with_bg = soup.find_all(style=re.compile(r'background(-image)?:\s*url'))
            for element in elements_with_bg:
                style = element.get('style', '')
                urls = re.findall(r'url\(["\']?([^"\']+)["\']?\)', style)
                for img_url in urls:
                    if not img_url.startswith('data:'):  # Pular data URIs aqui
                        absolute_url = urljoin(url, img_url)
                        if absolute_url not in seen_urls:
                            seen_urls.add(absolute_url)
                            image_data.append({
                                'url': absolute_url,
                                'type': 'css_background',
                                'alt': '',
                                'context': self._get_element_context(element)
                            })
            
            # 6. Data URIs em imgs
            for img in soup.find_all(['img', 'input']):
                src = img.get('src', '')
                if src.startswith('data:image'):
                    image_data.append({
                        'url': None,
                        'type': 'data_uri',
                        'content': src[:100] + '...',  # Preview
                        'alt': img.get('alt', ''),
                        'context': self._get_element_context(img),
                        'full_data': src  # Manter data URI completo
                    })
            
            # 7. Tags <video> poster - primeira frame
            for video in soup.find_all('video'):
                poster = video.get('poster')
                if poster:
                    absolute_url = urljoin(url, poster)
                    if absolute_url not in seen_urls:
                        seen_urls.add(absolute_url)
                        image_data.append({
                            'url': absolute_url,
                            'type': 'video_poster',
                            'alt': 'Video thumbnail',
                            'context': self._get_element_context(video)
                        })
            
            # 8. Open Graph e meta tags - imagens de compartilhamento
            meta_properties = ['og:image', 'twitter:image', 'image']
            for prop in meta_properties:
                meta = soup.find('meta', property=prop) or soup.find('meta', attrs={'name': prop})
                if meta and meta.get('content'):
                    absolute_url = urljoin(url, meta['content'])
                    if absolute_url not in seen_urls:
                        seen_urls.add(absolute_url)
                        image_data.append({
                            'url': absolute_url,
                            'type': 'meta_image',
                            'alt': f'Meta {prop}',
                            'context': 'Page metadata image'
                        })
            
            # Limitar quantidade total
            image_data = image_data[:self.max_images]
            
            return text, image_data
                
        except Exception as e:
            raise Exception(f"Erro ao buscar conteúdo da URL: {str(e)}")
    
    def _extract_image_info(self, element, base_url: str, source_type: str) -> Optional[Dict]:
        """
        Extrai informações de imagem de um elemento HTML
        """
        # Múltiplas formas de encontrar URL da imagem
        url_attrs = ['src', 'data-src', 'data-lazy-src', 'data-original', 
                     'data-srcset', 'srcset', 'data', 'content']
        
        img_url = None
        for attr in url_attrs:
            value = element.get(attr)
            if value:
                # Lidar com srcset
                if 'srcset' in attr:
                    # Pegar primeira URL do srcset
                    img_url = value.split(',')[0].split()[0]
                else:
                    img_url = value
                break
        
        if not img_url or img_url.startswith('#'):
            return None
        
        # Converter para URL absoluta
        absolute_url = urljoin(base_url, img_url)
        
        return {
            'url': absolute_url,
            'type': source_type,
            'alt': element.get('alt', ''),
            'title': element.get('title', ''),
            'context': self._get_element_context(element)
        }
    
    def _get_element_context(self, element) -> str:
        """
        Obtém contexto do elemento (texto ao redor, classe, etc.)
        """
        context_parts = []
        
        # Classes e ID
        if element.get('class'):
            context_parts.append(f"class: {' '.join(element['class'])}")
        if element.get('id'):
            context_parts.append(f"id: {element['id']}")
        
        # Texto do elemento pai
        parent = element.parent
        if parent:
            parent_text = parent.get_text(strip=True)[:100]
            if parent_text:
                context_parts.append(f"context: {parent_text}")
        
        return ' | '.join(context_parts)
    
    def process_image_data(self, image_info: Dict) -> Optional[Dict]:
        """
        Processa diferentes tipos de dados de imagem para envio ao GPT-4 Vision
        
        Args:
            image_info: Informações da imagem
            
        Returns:
            Dict com dados processados ou None se falhar
        """
        try:
            # SVG inline
            if image_info['type'] == 'svg_inline':
                # Converter SVG para imagem
                svg_content = image_info['content']
                return {
                    "type": "text",
                    "text": f"[SVG Inline - {image_info.get('context', '')}]\n{svg_content}"
                }
            
            # Data URI
            elif image_info['type'] == 'data_uri':
                return {
                    "type": "image_url",
                    "image_url": {
                        "url": image_info['full_data']
                    }
                }
            
            # URLs externas
            elif image_info.get('url'):
                return self.download_and_verify_image(image_info['url'])
            
            return None
            
        except Exception as e:
            print(f"Erro ao processar imagem: {str(e)}")
            return None
    
    def download_and_verify_image(self, image_url: str) -> Optional[Dict]:
        """
        Baixa e verifica se URL é realmente uma imagem através do Content-Type
        
        Args:
            image_url: URL para verificar e baixar
            
        Returns:
            Dict com dados da imagem ou None se não for imagem
        """
        try:
            # Fazer HEAD request primeiro para verificar tipo
            head_response = self.session.head(image_url, timeout=5, allow_redirects=True)
            content_type = head_response.headers.get('content-type', '').lower()
            
            # Lista expandida de MIME types de imagem
            image_mime_types = [
                'image/jpeg', 'image/jpg', 'image/png', 'image/gif', 
                'image/webp', 'image/svg+xml', 'image/bmp', 'image/tiff',
                'image/x-icon', 'image/vnd.microsoft.icon', 'image/avif',
                'image/heic', 'image/heif'
            ]
            
            # Verificar se é imagem pelo Content-Type
            is_image = any(mime in content_type for mime in image_mime_types)
            
            # Se não identificado pelo header, tentar GET parcial
            if not is_image and not content_type:
                response = self.session.get(image_url, stream=True, timeout=5)
                # Ler primeiros bytes para verificar magic numbers
                first_bytes = response.raw.read(12)
                is_image = self._check_image_magic_bytes(first_bytes)
                response.close()
            
            if not is_image:
                return None
            
            # Se for SVG, tratar como texto
            if 'svg' in content_type:
                response = self.session.get(image_url, timeout=10)
                return {
                    "type": "text",
                    "text": f"[SVG from {image_url}]\n{response.text[:1000]}"
                }
            
            # Para outras imagens, baixar e codificar
            response = self.session.get(image_url, timeout=10)
            response.raise_for_status()
            
            # Limitar tamanho
            if len(response.content) > 20 * 1024 * 1024:
                img = Image.open(BytesIO(response.content))
                img.thumbnail((1920, 1920), Image.Resampling.LANCZOS)
                
                buffer = BytesIO()
                img_format = 'JPEG' if img.mode in ['RGB', 'L'] else 'PNG'
                img.save(buffer, format=img_format, optimize=True, quality=85)
                image_data = buffer.getvalue()
            else:
                image_data = response.content
            
            # Codificar em base64
            base64_image = base64.b64encode(image_data).decode('utf-8')
            
            # Determinar MIME type correto
            if not content_type or content_type == 'application/octet-stream':
                content_type = self._guess_content_type(image_data)
            
            return {
                "type": "image_url",
                "image_url": {
                    "url": f"data:{content_type};base64,{base64_image}"
                }
            }
            
        except Exception as e:
            print(f"Erro ao processar {image_url}: {str(e)}")
            return None
    
    def _check_image_magic_bytes(self, data: bytes) -> bool:
        """
        Verifica magic bytes para identificar imagens
        """
        if len(data) < 4:
            return False
            
        # Magic bytes de formatos comuns
        magic_bytes = {
            b'\xFF\xD8\xFF': 'jpeg',
            b'\x89PNG': 'png',
            b'GIF87a': 'gif',
            b'GIF89a': 'gif',
            b'RIFF': 'webp',  # Precisa verificar WEBP depois
            b'<svg': 'svg',
            b'<?xml': 'svg',  # SVGs podem começar com XML declaration
            b'BM': 'bmp',
            b'II*\x00': 'tiff',
            b'MM\x00*': 'tiff'
        }
        
        for magic, format_name in magic_bytes.items():
            if data.startswith(magic):
                # Verificação especial para WebP
                if format_name == 'webp' and len(data) >= 12:
                    return data[8:12] == b'WEBP'
                return True
                
        return False
    
    def _guess_content_type(self, data: bytes) -> str:
        """
        Adivinha content-type baseado no conteúdo
        """
        if data.startswith(b'\xFF\xD8\xFF'):
            return 'image/jpeg'
        elif data.startswith(b'\x89PNG'):
            return 'image/png'
        elif data.startswith(b'GIF8'):
            return 'image/gif'
        elif data.startswith(b'RIFF') and len(data) > 12 and data[8:12] == b'WEBP':
            return 'image/webp'
        else:
            return 'image/jpeg'  # Padrão
    
    def analyze_with_gpt_vision(self, text_content: str, image_contents: List[Dict], 
                               custom_prompt: Optional[str] = None) -> Dict:
        """
        Analisa conteúdo de texto e imagens usando GPT-4 Vision
        
        Args:
            text_content: Conteúdo de texto extraído
            image_contents: Lista de dicts com imagens codificadas
            custom_prompt: Prompt customizado (opcional)
            
        Returns:
            Estrutura JSON com análise
        """
        # Prompt padrão para análise multimodal
        default_prompt = """
        Analise o conteúdo fornecido (texto e imagens) e estruture as informações em formato JSON.
        
        Para o TEXTO, identifique:
        - Principais tópicos/categorias
        - Entidades importantes (pessoas, organizações, lugares, datas)
        - Dados quantitativos
        
        Para as IMAGENS, analise:
        - O que cada imagem mostra (objetos, pessoas, cenários)
        - Texto presente nas imagens (OCR)
        - Gráficos, tabelas ou diagramas
        - Contexto e relevância para o conteúdo
        
        Retorne um JSON com a seguinte estrutura:
        {
            "summary": "resumo geral integrando texto e imagens",
            "main_topics": ["tópico1", "tópico2", ...],
            "entities": {
                "people": ["pessoa1", "pessoa2", ...],
                "organizations": ["org1", "org2", ...],
                "locations": ["local1", "local2", ...],
                "dates": ["data1", "data2", ...]
            },
            "image_analysis": [
                {
                    "image_index": 0,
                    "description": "descrição detalhada da imagem",
                    "text_in_image": "texto extraído da imagem se houver",
                    "data_visualization": "descrição de gráficos/tabelas se houver",
                    "key_elements": ["elemento1", "elemento2", ...]
                }
            ],
            "structured_data": [
                {
                    "source": "text/image",
                    "category": "categoria",
                    "title": "título",
                    "description": "descrição",
                    "value": "valor se aplicável",
                    "metadata": {}
                }
            ],
            "key_insights": ["insight1 integrando texto e imagens", "insight2", ...]
        }
        
        Integre as informações de texto e imagens para uma análise completa.
        """
        
        prompt = custom_prompt or default_prompt
        
        # Construir mensagens com conteúdo multimodal
        content_parts = [
            {
                "type": "text",
                "text": f"{prompt}\n\nTexto extraído da página:\n\n{text_content[:4000]}"
            }
        ]
        
        # Adicionar imagens
        if image_contents:
            content_parts.append({
                "type": "text",
                "text": f"\n\nForam encontradas {len(image_contents)} imagens na página. Analise cada uma:"
            })
            content_parts.extend(image_contents)
        
        try:
            response = self.client.chat.completions.create(
                model="gpt-4o",  # ou gpt-4-vision-preview
                #model="gpt-4-vision-preview"
                messages=[
                    {
                        "role": "system", 
                        "content": "Você é um assistente especializado em análise multimodal de conteúdo web."
                    },
                    {
                        "role": "user",
                        "content": content_parts
                    }
                ],
                temperature=0.3,
                max_tokens=4096,
                response_format={"type": "json_object"}
            )
            
            return json.loads(response.choices[0].message.content)
            
        except Exception as e:
            raise Exception(f"Erro na análise GPT Vision: {str(e)}")
    
    def json_to_dataframe(self, json_data: Dict) -> pd.DataFrame:
        """
        Converte a estrutura JSON em DataFrame incluindo análise de imagens
        
        Args:
            json_data: Dados JSON estruturados
            
        Returns:
            DataFrame pandas
        """
        rows = []
        
        # Adicionar resumo
        if 'summary' in json_data:
            rows.append({
                'tipo': 'resumo',
                'categoria': 'geral',
                'fonte': 'texto+imagem',
                'conteudo': json_data['summary'],
                'detalhes': ''
            })
        
        # Adicionar tópicos principais
        if 'main_topics' in json_data:
            for topic in json_data['main_topics']:
                rows.append({
                    'tipo': 'tópico',
                    'categoria': 'principal',
                    'fonte': 'texto',
                    'conteudo': topic,
                    'detalhes': ''
                })
        
        # Adicionar entidades
        if 'entities' in json_data:
            for entity_type, entities in json_data['entities'].items():
                for entity in entities:
                    rows.append({
                        'tipo': 'entidade',
                        'categoria': entity_type,
                        'fonte': 'texto',
                        'conteudo': entity,
                        'detalhes': ''
                    })
        
        # Adicionar análise de imagens
        if 'image_analysis' in json_data:
            for img_analysis in json_data['image_analysis']:
                rows.append({
                    'tipo': 'imagem',
                    'categoria': 'análise_visual',
                    'fonte': f"imagem_{img_analysis.get('image_index', 0)}",
                    'conteudo': img_analysis.get('description', ''),
                    'detalhes': json.dumps({
                        'text_in_image': img_analysis.get('text_in_image', ''),
                        'data_visualization': img_analysis.get('data_visualization', ''),
                        'key_elements': img_analysis.get('key_elements', [])
                    }, ensure_ascii=False)
                })
        
        # Adicionar dados estruturados
        if 'structured_data' in json_data:
            for item in json_data['structured_data']:
                rows.append({
                    'tipo': 'dado_estruturado',
                    'categoria': item.get('category', ''),
                    'fonte': item.get('source', 'texto'),
                    'conteudo': item.get('title', ''),
                    'detalhes': json.dumps({
                        'description': item.get('description', ''),
                        'value': item.get('value', ''),
                        'metadata': item.get('metadata', {})
                    }, ensure_ascii=False)
                })
        
        # Adicionar insights
        if 'key_insights' in json_data:
            for insight in json_data['key_insights']:
                rows.append({
                    'tipo': 'insight',
                    'categoria': 'análise',
                    'fonte': 'texto+imagem',
                    'conteudo': insight,
                    'detalhes': ''
                })
        
        return pd.DataFrame(rows)
    
    def analyze_url(self, url: str, custom_prompt: Optional[str] = None, 
                   analyze_images: bool = True) -> pd.DataFrame:
        """
        Processo completo: busca URL, analisa texto e imagens com GPT Vision
        
        Args:
            url: URL para analisar
            custom_prompt: Prompt customizado (opcional)
            analyze_images: Se deve analisar imagens (padrão: True)
            
        Returns:
            DataFrame com análise estruturada
        """
        print(f"1. Buscando conteúdo de: {url}")
        text_content, image_data_list = self.fetch_url_content(url)
        
        print(f"2. Conteúdo obtido:")
        print(f"   - Texto: {len(text_content)} caracteres")
        print(f"   - Elementos visuais encontrados: {len(image_data_list)}")
        
        # Mostrar tipos de imagens encontradas
        image_types = {}
        for img in image_data_list:
            img_type = img['type']
            image_types[img_type] = image_types.get(img_type, 0) + 1
        
        for img_type, count in image_types.items():
            print(f"     • {img_type}: {count}")
        
        # Processar imagens se habilitado
        image_contents = []
        if analyze_images and image_data_list:
            print("\n3. Processando elementos visuais...")
            for i, img_data in enumerate(image_data_list):
                print(f"   - Processando {i+1}/{len(image_data_list)}: {img_data['type']}", end='')
                
                if img_data.get('url'):
                    print(f" - {img_data['url'][:60]}...")
                else:
                    print(f" - {img_data.get('alt', 'embedded content')}")
                
                processed_img = self.process_image_data(img_data)
                if processed_img:
                    # Adicionar contexto da imagem
                    if processed_img['type'] == 'text':
                        processed_img['text'] = f"Context: {img_data.get('context', 'N/A')}\n{processed_img['text']}"
                    image_contents.append(processed_img)
                    
            print(f"\n   - Elementos processados com sucesso: {len(image_contents)}")
        
        print("\n4. Analisando com GPT-4 Vision...")
        analysis = self.analyze_with_gpt_vision(text_content, image_contents, custom_prompt)
        
        print("5. Convertendo para DataFrame...")
        df = self.json_to_dataframe(analysis)
        
        # Adicionar metadados
        df['url_origem'] = url
        df['timestamp'] = pd.Timestamp.now()
        df['num_imagens_analisadas'] = len(image_contents)
        
        return df


# Exemplo de uso
def main():
    # Configurar chave da API
    api_key = os.getenv('OPENAI_API_KEY')
    if not api_key:
        print("\n❌ OPENAI_API_KEY não encontrada!")
        print("\nOpções para configurar:")
        print("1. Exportar diretamente:")
        print("   export OPENAI_API_KEY='sua-chave-aqui'")
        print("\n2. Criar arquivo .env no diretório atual ou pai com:")
        print("   OPENAI_API_KEY=sua-chave-aqui")
        print("\n3. Instalar python-dotenv se usar .env:")
        print("   python3 -m pip install python-dotenv")
        return
    
    # Criar analisador
    analyzer = GPTURLAnalyzer(api_key, max_images=5)
    
    # URLs de exemplo com conteúdo visual
    urls_exemplo = [
            "https://storymaps.arcgis.com/stories/c3311c4fe7d349feb799e76113ced174"
    ]
    
    for url in urls_exemplo:
        try:
            print(f"\n{'='*60}")
            print(f"Analisando: {url}")
            print('='*60)
            
            # Analisar URL com imagens
            df = analyzer.analyze_url(url, analyze_images=True)
            
            # Mostrar resultados
            print(f"\n6. Análise concluída!")
            print(f"Total de registros: {len(df)}")
            
            # Filtrar análises de imagem
            img_analysis = df[df['tipo'] == 'imagem']
            if not img_analysis.empty:
                print(f"\nAnálise de imagens ({len(img_analysis)} registros):")
                for _, row in img_analysis.iterrows():
                    print(f"\n- {row['conteudo']}")
                    detalhes = json.loads(row['detalhes'])
                    if detalhes.get('text_in_image'):
                        print(f"  Texto na imagem: {detalhes['text_in_image']}")
                    if detalhes.get('data_visualization'):
                        print(f"  Visualização: {detalhes['data_visualization']}")
            
            # Salvar resultados
            output_file = f"analise_{urlparse(url).netloc.replace('.', '_')}.csv"
            df.to_csv(output_file, index=False, encoding='utf-8')
            print(f"\nResultados salvos em: {output_file}")
            
        except Exception as e:
            print(f"Erro ao analisar {url}: {str(e)}")


# Exemplo de prompt customizado para análise focada em imagens
def analyze_visual_content():
    api_key = os.getenv('OPENAI_API_KEY')
    analyzer = GPTURLAnalyzer(api_key)
    
    custom_prompt = """
    Foque principalmente na análise visual do conteúdo. Para cada imagem:
    
    1. Descreva detalhadamente o que está sendo mostrado
    2. Extraia QUALQUER texto visível (OCR completo)
    3. Se houver gráficos ou dados, extraia os valores numéricos
    4. Identifique produtos, marcas, logotipos
    5. Analise cores, composição e elementos de design
    6. Determine o propósito/contexto da imagem
    
    Priorize informações visuais sobre o texto da página.
    """
    
    url = "https://example.com/catalogo-visual"
    df = analyzer.analyze_url(url, custom_prompt=custom_prompt)
    
    # Análise específica de conteúdo visual
    visual_data = df[df['fonte'].str.contains('imagem')]
    print(f"\nConteúdo visual extraído: {len(visual_data)} elementos")


# Exemplo de uso avançado focado em detecção robusta de imagens
def test_image_detection():
    """
    Testa a detecção de imagens em diferentes formatos e fontes
    """
    api_key = os.getenv('OPENAI_API_KEY')
    analyzer = GPTURLAnalyzer(api_key, max_images=20)
    
    # Sites de teste com diferentes tipos de imagens
    test_sites = {
        "E-commerce moderno": "https://example-shop.com",
        "Site com SVGs": "https://example-icons.com",
        "Dashboard com gráficos": "https://example-analytics.com",
        "Site com lazy loading": "https://example-gallery.com",
        "Blog com infográficos": "https://example-blog.com"
    }
    
    for site_type, url in test_sites.items():
        print(f"\n{'='*60}")
        print(f"Testando: {site_type}")
        print('='*60)
        
        try:
            # Buscar apenas metadados primeiro
            _, image_data = analyzer.fetch_url_content(url)
            
            print(f"\nResumo de detecção:")
            print(f"Total de elementos visuais: {len(image_data)}")
            
            # Agrupar por tipo
            by_type = {}
            for img in image_data:
                img_type = img['type']
                by_type[img_type] = by_type.get(img_type, [])
                by_type[img_type].append(img)
            
            # Mostrar estatísticas
            for img_type, images in by_type.items():
                print(f"\n{img_type.upper()} ({len(images)} encontrados):")
                for i, img in enumerate(images[:3]):  # Mostrar primeiros 3
                    if img.get('url'):
                        print(f"  - {img['url'][:60]}...")
                    else:
                        print(f"  - {img.get('alt', 'Conteúdo embutido')[:60]}...")
                    if img.get('context'):
                        print(f"    Contexto: {img['context'][:50]}...")
                        
                if len(images) > 3:
                    print(f"  ... e mais {len(images) - 3}")
                    
        except Exception as e:
            print(f"Erro: {str(e)}")


# Exemplo com foco em extração de dados de gráficos e visualizações
def analyze_data_visualizations():
    """
    Análise específica para dashboards e visualizações de dados
    """
    api_key = os.getenv('OPENAI_API_KEY')
    analyzer = GPTURLAnalyzer(api_key)
    
    custom_prompt = """
    Foque na análise de visualizações de dados. Para cada elemento visual:
    
    1. GRÁFICOS E CHARTS:
       - Tipo de gráfico (barras, linha, pizza, etc.)
       - Dados exatos mostrados (valores, labels, legendas)
       - Tendências e insights dos dados
       - Período temporal se aplicável
    
    2. TABELAS E GRIDS:
       - Headers e estrutura
       - Dados numéricos completos
       - Totais e subtotais
    
    3. KPIs E MÉTRICAS:
       - Valores exatos
       - Comparações (vs período anterior, meta, etc.)
       - Indicadores visuais (setas, cores)
    
    4. INFOGRÁFICOS:
       - Fluxo de informação
       - Dados e estatísticas
       - Relações entre elementos
    
    Extraia TODOS os dados numéricos visíveis com precisão.
    """
    
    url = "https://example.com/dashboard"
    df = analyzer.analyze_url(url, custom_prompt=custom_prompt)
    
    # Filtrar apenas dados quantitativos
    quantitative_data = df[df['categoria'].str.contains('dado|métrica|estatística', case=False, na=False)]
    
    print("\nDados Quantitativos Extraídos:")
    for _, row in quantitative_data.iterrows():
        print(f"\n{row['conteudo']}")
        if row['detalhes']:
            details = json.loads(row['detalhes'])
            if details.get('value'):
                print(f"  Valor: {details['value']}")


if __name__ == "__main__":
    # Instalar dependências necessárias:
    # python3 -m pip install openai pandas requests beautifulsoup4 pillow lxml
    
    print("Verificando dependências...")
    try:
        import openai
        import pandas
        import requests
        import bs4
        import PIL
        import lxml
        print("✓ Todas as dependências instaladas!")
    except ImportError as e:
        print(f"✗ Dependência faltando: {e}")
        print("\nInstale com: python3 -m pip install openai pandas requests beautifulsoup4 pillow lxml")
        exit(1)
    
    main()

