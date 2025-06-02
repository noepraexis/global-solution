#!/usr/bin/env python3
"""
Glide Incident Finder - A incident search tool for Glide Number lookups.

This module provides a consistent, scalable architecture for searching and
retrieving incident information using the Google Custom Search API.
"""

import argparse
import json
import logging
import sys
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Any, Union
from urllib.parse import urlencode, quote

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry  # Importação correta do Retry


# Configuration Management
class Config:
    """Centralized configuration management."""

    # API Configuration
    API_KEY = "AIzaSyDPDGPX_v7hoWkLXyclBoFa-ZIpfOGefNE"
    SEARCH_ENGINE_ID = "45930a97174884d08"
    BASE_URL = "https://content-customsearch.googleapis.com/customsearch/v1"

    # Request Configuration
    DEFAULT_TIMEOUT = 30
    MAX_RETRIES = 3
    BACKOFF_FACTOR = 0.3

    # Logging Configuration
    LOG_LEVEL = logging.INFO
    LOG_FORMAT = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    # Default Headers
    DEFAULT_HEADERS = {
        'accept': '*/*',
        'accept-language': 'en-US,en;q=0.9,pt;q=0.8',
        'dnt': '1',
        'priority': 'u=1, i',
        'referer': 'https://content-customsearch.googleapis.com/static/proxy.html',
        'sec-ch-ua': '"Chromium";v="136", "Google Chrome";v="136", "Not.A/Brand";v="99"',
        'sec-ch-ua-mobile': '?0',
        'sec-ch-ua-platform': '"Windows"',
        'sec-fetch-dest': 'empty',
        'sec-fetch-mode': 'cors',
        'sec-fetch-site': 'same-origin',
        'user-agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
        'x-origin': 'https://glidenumber.net',
        'x-referer': 'https://glidenumber.net',
        'x-requested-with': 'XMLHttpRequest'
    }


# Data Models
@dataclass
class SearchResult:
    """Represents a single search result."""
    title: str
    link: str
    snippet: str
    display_link: str
    formatted_url: str
    html_title: Optional[str] = None
    html_snippet: Optional[str] = None
    mime_type: Optional[str] = None
    file_format: Optional[str] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'SearchResult':
        """Create SearchResult from API response dictionary."""
        return cls(
            title=data.get('title', ''),
            link=data.get('link', ''),
            snippet=data.get('snippet', ''),
            display_link=data.get('displayLink', ''),
            formatted_url=data.get('formattedUrl', ''),
            html_title=data.get('htmlTitle'),
            html_snippet=data.get('htmlSnippet'),
            mime_type=data.get('mime'),
            file_format=data.get('fileFormat')
        )


@dataclass
class SearchResponse:
    """Represents the complete search response."""
    total_results: int
    search_time: float
    items: List[SearchResult] = field(default_factory=list)
    queries: Dict[str, Any] = field(default_factory=dict)
    search_information: Dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'SearchResponse':
        """Create SearchResponse from API response dictionary."""
        search_info = data.get('searchInformation', {})
        items = [SearchResult.from_dict(item) for item in data.get('items', [])]

        return cls(
            total_results=int(search_info.get('totalResults', 0)),
            search_time=float(search_info.get('searchTime', 0)),
            items=items,
            queries=data.get('queries', {}),
            search_information=search_info
        )


class SearchError(Exception):
    """Custom exception for search-related errors."""
    pass


class APIError(SearchError):
    """Exception for API-related errors."""
    def __init__(self, message: str, status_code: Optional[int] = None, response_body: Optional[str] = None):
        super().__init__(message)
        self.status_code = status_code
        self.response_body = response_body


# Service Layer
class IncidentSearchService:
    """Service for searching incidents using Google Custom Search API."""

    def __init__(self, config: Optional[Config] = None):
        """Initialize the search service."""
        self.config = config or Config()
        self.logger = self._setup_logger()
        self.session = self._create_session()

    def _setup_logger(self) -> logging.Logger:
        """Set up logging configuration."""
        logger = logging.getLogger(self.__class__.__name__)
        logger.setLevel(self.config.LOG_LEVEL)

        if not logger.handlers:
            handler = logging.StreamHandler(sys.stdout)
            handler.setFormatter(logging.Formatter(self.config.LOG_FORMAT))
            logger.addHandler(handler)

        return logger

    def _create_session(self) -> requests.Session:
        """Create a requests session with retry logic."""
        session = requests.Session()

        retry_strategy = Retry(
            total=self.config.MAX_RETRIES,
            backoff_factor=self.config.BACKOFF_FACTOR,
            status_forcelist=[429, 500, 502, 503, 504],
            allowed_methods=["GET"]
        )

        adapter = HTTPAdapter(max_retries=retry_strategy)
        session.mount("http://", adapter)
        session.mount("https://", adapter)

        session.headers.update(self.config.DEFAULT_HEADERS)

        return session

    def search_incident(self, glide_number: str, start: int = 1) -> SearchResponse:
        """
        Search for an incident by Glide Number.

        Args:
            glide_number: The Glide Number to search for
            start: The starting index for results (1-based)

        Returns:
            SearchResponse containing the search results

        Raises:
            APIError: If the API request fails
            SearchError: If there's an error processing the search
        """
        self.logger.info(f"Searching for incident: {glide_number}")

        # Validate input
        if not glide_number:
            raise ValueError("Glide Number cannot be empty")

        # Build query parameters
        params = {
            'start': start,
            'cx': self.config.SEARCH_ENGINE_ID,
            'exactTerms': f'"{glide_number}"',
            'key': self.config.API_KEY
        }

        try:
            # Make the request
            response = self.session.get(
                self.config.BASE_URL,
                params=params,
                timeout=self.config.DEFAULT_TIMEOUT
            )

            # Check for HTTP errors
            if response.status_code != 200:
                self.logger.error(f"API request failed with status {response.status_code}")
                raise APIError(
                    f"API request failed with status {response.status_code}",
                    status_code=response.status_code,
                    response_body=response.text
                )

            # Parse response
            data = response.json()
            search_response = SearchResponse.from_dict(data)

            self.logger.info(
                f"Search completed. Found {search_response.total_results} results "
                f"in {search_response.search_time:.3f} seconds"
            )

            return search_response

        except requests.RequestException as e:
            self.logger.error(f"Request error: {str(e)}")
            raise APIError(f"Request failed: {str(e)}")
        except json.JSONDecodeError as e:
            self.logger.error(f"Failed to parse response: {str(e)}")
            raise SearchError(f"Invalid response format: {str(e)}")
        except Exception as e:
            self.logger.error(f"Unexpected error: {str(e)}")
            raise SearchError(f"Search failed: {str(e)}")

    def search_all_pages(self, glide_number: str, max_results: int = 100) -> List[SearchResult]:
        """
        Search for all results across multiple pages.

        Args:
            glide_number: The Glide Number to search for
            max_results: Maximum number of results to retrieve

        Returns:
            List of all search results
        """
        all_results = []
        start = 1

        while len(all_results) < max_results:
            try:
                response = self.search_incident(glide_number, start)

                if not response.items:
                    break

                all_results.extend(response.items)

                # Check if there are more results
                next_query = response.queries.get('nextPage')
                if not next_query:
                    break

                start = next_query[0].get('startIndex', start + 10)

            except SearchError:
                self.logger.warning(f"Failed to fetch page starting at {start}")
                break

        return all_results[:max_results]


# Output Formatters
class OutputFormatter:
    """Base class for output formatting."""

    def format(self, response: SearchResponse) -> str:
        """Format the search response."""
        raise NotImplementedError


class JSONFormatter(OutputFormatter):
    """Format output as JSON."""

    def format(self, response: SearchResponse) -> str:
        """Format response as pretty-printed JSON."""
        data = {
            'total_results': response.total_results,
            'search_time': response.search_time,
            'items': [
                {
                    'title': item.title,
                    'link': item.link,
                    'snippet': item.snippet,
                    'display_link': item.display_link
                }
                for item in response.items
            ]
        }
        return json.dumps(data, indent=2, ensure_ascii=False)


class TextFormatter(OutputFormatter):
    """Format output as human-readable text."""

    def format(self, response: SearchResponse) -> str:
        """Format response as readable text."""
        lines = [
            f"Found {response.total_results} results in {response.search_time:.3f} seconds",
            "-" * 80
        ]

        for i, item in enumerate(response.items, 1):
            lines.extend([
                f"\n[{i}] {item.title}",
                f"    URL: {item.link}",
                f"    {item.snippet}",
                "-" * 80
            ])

        return "\n".join(lines)


# CLI Interface
class CLIHandler:
    """Command-line interface handler."""

    def __init__(self):
        self.service = IncidentSearchService()
        self.formatters = {
            'json': JSONFormatter(),
            'text': TextFormatter()
        }

    def create_parser(self) -> argparse.ArgumentParser:
        """Create and configure argument parser."""
        parser = argparse.ArgumentParser(
            description="Search for incidents using Glide Numbers",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s WF-2024-000158-BRA
  %(prog)s WF-2024-000158-BRA --format json
  %(prog)s WF-2024-000158-BRA --all-pages --max-results 50
  %(prog)s WF-2024-000158-BRA --verbose
            """
        )

        parser.add_argument(
            'glide_number',
            help='The Glide Number to search for'
        )

        parser.add_argument(
            '--format', '-f',
            choices=['json', 'text'],
            default='text',
            help='Output format (default: text)'
        )

        parser.add_argument(
            '--start',
            type=int,
            default=1,
            help='Starting index for results (default: 1)'
        )

        parser.add_argument(
            '--all-pages',
            action='store_true',
            help='Retrieve results from all pages'
        )

        parser.add_argument(
            '--max-results',
            type=int,
            default=100,
            help='Maximum number of results when using --all-pages (default: 100)'
        )

        parser.add_argument(
            '--verbose', '-v',
            action='store_true',
            help='Enable verbose logging'
        )

        return parser

    def run(self, args: Optional[List[str]] = None) -> int:
        """Run the CLI application."""
        parser = self.create_parser()
        parsed_args = parser.parse_args(args)

        # Configure logging level
        if parsed_args.verbose:
            logging.getLogger().setLevel(logging.DEBUG)

        try:
            if parsed_args.all_pages:
                # Fetch all pages
                results = self.service.search_all_pages(
                    parsed_args.glide_number,
                    parsed_args.max_results
                )
                response = SearchResponse(
                    total_results=len(results),
                    search_time=0,
                    items=results
                )
            else:
                # Single page search
                response = self.service.search_incident(
                    parsed_args.glide_number,
                    parsed_args.start
                )

            # Format and output results
            formatter = self.formatters[parsed_args.format]
            print(formatter.format(response))

            return 0

        except Exception as e:
            print(f"Error: {str(e)}", file=sys.stderr)
            if parsed_args.verbose:
                import traceback
                traceback.print_exc()
            return 1


def main():
    """Main entry point."""
    cli = CLIHandler()
    sys.exit(cli.run())


if __name__ == "__main__":
    main()
if __name__ == "__main__":
    main()

