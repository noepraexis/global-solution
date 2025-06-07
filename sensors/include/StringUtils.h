/**
 * @file StringUtils.h
 * @brief Utilitários otimizados para manipulação de strings.
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <Arduino.h>
#include <string.h>

namespace StringUtils {

/**
 * @brief Copia uma string de forma segura e otimizada.
 *
 * Esta função copia uma string para um buffer de destino com tamanho fixo,
 * garantindo a terminação nula mesmo em casos de truncamento.
 * Implementada para melhor desempenho usando memcpy com cálculo preciso.
 *
 * @param dest Buffer de destino
 * @param src String de origem
 * @param destSize Tamanho total do buffer de destino (incluindo espaço para \0)
 */
inline void safeCopyString(char* dest, const char* src, size_t destSize) {
    if (!dest || !src || destSize == 0) return;

    // Determina o tamanho a ser copiado
    size_t maxLen = destSize - 1;
    size_t srcLen = strlen(src);
    size_t copyLen = (srcLen < maxLen) ? srcLen : maxLen;

    // Copia os bytes e adiciona terminador nulo
    memcpy(dest, src, copyLen);
    dest[copyLen] = '\0';
}

} // namespace StringUtils

#endif // STRING_UTILS_H