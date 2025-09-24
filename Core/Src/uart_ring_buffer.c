/*
 * uart_ring_buffer.c
 *
 *  Created on: Sep 22, 2025
 *      Author: hendra-saputro
 */


#include "uart_ring_buffer.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/**** Configuration Section ****/
#define UART_INSTANCE &huart2
#define DEFAULT_TIMEOUT_MS 500
#define UART_BUFFER_SIZE 1024  // Make this configurable

/**** External Dependencies ****/
extern UART_HandleTypeDef huart2;

/**** Private Variables ****/
static RingBuffer_TypeDef rx_buffer = {{0}, 0, 0};
static RingBuffer_TypeDef tx_buffer = {{0}, 0, 0};
static volatile uint32_t timeout_start;

/**** Private Function Prototypes ****/
static UART_ErrorTypeDef StoreChar(uint8_t c, RingBuffer_TypeDef *buffer);
static bool IsTimeOutExpired(uint32_t timeout_ms);
static void ResetTimeout(void);
static size_t FindStringInBuffer(const char *str, const char *buffer, size_t buffer_len);
static UART_ErrorTypeDef WaitForData(uint32_t timeout_ms);

/**** Public Functions ****/

/**
 * @brief Initialize UART ring buffer
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_RingBuff_Init(void)
{
    // Clear buffers
    memset(&rx_buffer, 0, sizeof(RingBuffer_TypeDef));
    memset(&tx_buffer, 0, sizeof(RingBuffer_TypeDef));

    // Enable UART interrupts
    __HAL_UART_ENABLE_IT(UART_INSTANCE, UART_IT_ERR);
    __HAL_UART_ENABLE_IT(UART_INSTANCE, UART_IT_RXNE);

    return UART_SUCCESS;
}

/**
 * @brief Read a single character from RX buffer
 * @param c Pointer to store the character
 * @return UART_SUCCESS on success, UART_ERROR_BUFFER_EMPTY if no data
 */
UART_ErrorTypeDef UART_ReadChar(uint8_t *c)
{
    if (c == NULL) {
        return UART_ERROR_INVALID_PARAM;
    }

    // Check if buffer is empty
    if (rx_buffer.head == rx_buffer.tail) {
        return UART_ERROR_BUFFER_EMPTY;
    }

    *c = rx_buffer.buffer[rx_buffer.tail];
    rx_buffer.tail = (rx_buffer.tail + 1) % UART_BUFFER_SIZE;

    return UART_SUCCESS;
}

/**
 * @brief Write a single character to TX buffer
 * @param c Character to write
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_WriteChar(uint8_t c)
{
    uint16_t next_head = (tx_buffer.head + 1) % UART_BUFFER_SIZE;

    // Wait if buffer is full (with timeout)
    ResetTimeout();
    while (next_head == tx_buffer.tail) {
        if (IsTimeOutExpired(DEFAULT_TIMEOUT_MS)) {
            return UART_ERROR_TIMEOUT;
        }
    }

    tx_buffer.buffer[tx_buffer.head] = c;
    tx_buffer.head = next_head;

    // Enable TX interrupt
    __HAL_UART_ENABLE_IT(UART_INSTANCE, UART_IT_TXE);

    return UART_SUCCESS;
}

/**
 * @brief Send string via UART
 * @param str String to send (null-terminated)
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_SendString(const char *str)
{
    if (str == NULL) {
        return UART_ERROR_INVALID_PARAM;
    }

    while (*str) {
    	UART_ErrorTypeDef result = UART_WriteChar(*str);
        if (result != UART_SUCCESS) {
            return result;
        }
        str++;
    }

    return UART_SUCCESS;
}

/**
 * @brief Check available data in RX buffer
 * @return Number of bytes available
 */
uint16_t UART_Available(void)
{
    return (UART_BUFFER_SIZE + rx_buffer.head - rx_buffer.tail) % UART_BUFFER_SIZE;
}

/**
 * @brief Peek at next character without removing it
 * @param c Pointer to store the character
 * @return UART_SUCCESS on success, UART_ERROR_BUFFER_EMPTY if no data
 */
UART_ErrorTypeDef UART_Peek(uint8_t *c)
{
    if (c == NULL) {
        return UART_ERROR_INVALID_PARAM;
    }

    if (rx_buffer.head == rx_buffer.tail) {
        return UART_ERROR_BUFFER_EMPTY;
    }

    *c = rx_buffer.buffer[rx_buffer.tail];
    return UART_SUCCESS;
}

/**
 * @brief Flush RX buffer
 */
void UART_FlushRX(void)
{
    __disable_irq();
    rx_buffer.head = 0;
    rx_buffer.tail = 0;
    memset(rx_buffer.buffer, 0, UART_BUFFER_SIZE);
    __enable_irq();
}

/**
 * @brief Wait for specific string with timeout
 * @param str String to wait for
 * @param timeout_ms Timeout in milliseconds
 * @return UART_SUCCESS if found, error code otherwise
 */
UART_ErrorTypeDef UART_WaitForString(const char *str, uint32_t timeout_ms)
{
    if (str == NULL || strlen(str) == 0) {
        return UART_ERROR_INVALID_PARAM;
    }

    size_t str_len = strlen(str);
    size_t match_pos = 0;

    ResetTimeout();

    while (match_pos < str_len) {
        // Wait for data with timeout
        if (WaitForData(timeout_ms) != UART_SUCCESS) {
            return UART_ERROR_TIMEOUT;
        }

        uint8_t c;
        if (UART_ReadChar(&c) != UART_SUCCESS) {
            continue;
        }

        if (c == str[match_pos]) {
            match_pos++;
        } else if (c == str[0]) {
            match_pos = 1;  // Start over if we found first character
        } else {
            match_pos = 0;  // Reset if no match
        }

        // Reset timeout on each character match
        if (match_pos > 0) {
            ResetTimeout();
        }
    }

    return UART_SUCCESS;
}

/**
 * @brief Copy data until specified string is found
 * @param end_str String that marks the end
 * @param buffer Buffer to copy data into
 * @param buffer_size Size of the destination buffer
 * @param timeout_ms Timeout in milliseconds
 * @return UART_SUCCESS if found, error code otherwise
 */
UART_ErrorTypeDef UART_CopyUntil(const char *end_str, char *buffer, size_t buffer_size, uint32_t timeout_ms)
{
    if (end_str == NULL || buffer == NULL || buffer_size == 0) {
        return UART_ERROR_INVALID_PARAM;
    }

    size_t str_len = strlen(end_str);
    size_t buffer_pos = 0;
    size_t match_pos = 0;

    ResetTimeout();

    while (buffer_pos < buffer_size - 1) {  // Leave space for null terminator
        if (WaitForData(timeout_ms) != UART_SUCCESS) {
            return UART_ERROR_TIMEOUT;
        }

        uint8_t c;
        if (UART_ReadChar(&c) != UART_SUCCESS) {
            continue;
        }

        buffer[buffer_pos++] = c;

        // Check for end string match
        if (c == end_str[match_pos]) {
            match_pos++;
            if (match_pos == str_len) {
                // Found the end string
                buffer[buffer_pos] = '\0';
                return UART_SUCCESS;
            }
        } else if (c == end_str[0]) {
            match_pos = 1;
        } else {
            match_pos = 0;
        }
    }

    buffer[buffer_pos] = '\0';
    return UART_ERROR_BUFFER_FULL;
}

/**
 * @brief Extract data between two strings
 * @param start_str Start delimiter
 * @param end_str End delimiter
 * @param source Source buffer
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_ExtractBetween(const char *start_str, const char *end_str,
                                  const char *source, char *dest, size_t dest_size)
{
    if (start_str == NULL || end_str == NULL || source == NULL || dest == NULL || dest_size == 0) {
        return UART_ERROR_INVALID_PARAM;
    }

    size_t source_len = strlen(source);

    // Find start string
    size_t start_pos = FindStringInBuffer(start_str, source, source_len);
    if (start_pos == SIZE_MAX) {
        return UART_ERROR_NOT_FOUND;
    }
    start_pos += strlen(start_str);

    // Find end string after start position
    size_t end_pos = FindStringInBuffer(end_str, source + start_pos, source_len - start_pos);
    if (end_pos == SIZE_MAX) {
        return UART_ERROR_NOT_FOUND;
    }
    end_pos += start_pos;

    // Copy data between strings
    size_t copy_len = end_pos - start_pos;
    if (copy_len >= dest_size) {
        copy_len = dest_size - 1;
    }

    memcpy(dest, source + start_pos, copy_len);
    dest[copy_len] = '\0';

    return UART_SUCCESS;
}

/**
 * @brief UART ISR handler - call this from your UART interrupt
 * @param huart UART handle
 */
void UART_ISR_Handler(UART_HandleTypeDef *huart)
{
    if (huart != UART_INSTANCE) {
        return;
    }

    uint32_t isr_flags = READ_REG(huart->Instance->ISR);
    uint32_t cr1_flags = READ_REG(huart->Instance->CR1);

    // Handle RX interrupt
    if ((isr_flags & USART_ISR_RXNE) && (cr1_flags & USART_CR1_RXNEIE)) {
        // Clear flags by reading SR then DR
        (void)huart->Instance->ISR;
        uint8_t received_char = (uint8_t)huart->Instance->RDR;
        StoreChar(received_char, &rx_buffer);
    }

    // Handle TX interrupt
    if ((isr_flags & USART_ISR_TXE) && (cr1_flags & USART_CR1_TXEIE)) {
        if (tx_buffer.head == tx_buffer.tail) {
            // Buffer empty, disable TX interrupt
            __HAL_UART_DISABLE_IT(huart, UART_IT_TXE);
        } else {
            // Send next character
            uint8_t c = tx_buffer.buffer[tx_buffer.tail];
            tx_buffer.tail = (tx_buffer.tail + 1) % UART_BUFFER_SIZE;

            (void)huart->Instance->ISR;
            huart->Instance->TDR = c;
        }
    }
}

/**** Private Functions ****/

/**
 * @brief Store character in ring buffer
 * @param c Character to store
 * @param buffer Ring buffer
 * @return UART_SUCCESS on success, UART_ERROR_BUFFER_FULL if buffer is full
 */
static UART_ErrorTypeDef StoreChar(uint8_t c, RingBuffer_TypeDef *buffer)
{
    uint16_t next_head = (buffer->head + 1) % UART_BUFFER_SIZE;

    // Check for buffer overflow
    if (next_head == buffer->tail) {
        return UART_ERROR_BUFFER_FULL;  // Buffer overflow
    }

    buffer->buffer[buffer->head] = c;
    buffer->head = next_head;

    return UART_SUCCESS;
}

/**
 * @brief Check if timeout has expired
 * @param timeout_ms Timeout value in milliseconds
 * @return true if timeout expired, false otherwise
 */
static bool IsTimeOutExpired(uint32_t timeout_ms)
{
    return (HAL_GetTick() - timeout_start) >= timeout_ms;
}

/**
 * @brief Reset timeout counter
 */
static void ResetTimeout(void)
{
    timeout_start = HAL_GetTick();
}

/**
 * @brief Find string in buffer
 * @param str String to find
 * @param buffer Buffer to search in
 * @param buffer_len Length of buffer
 * @return Position of string or SIZE_MAX if not found
 */
static size_t FindStringInBuffer(const char *str, const char *buffer, size_t buffer_len)
{
    if (str == NULL || buffer == NULL) {
        return SIZE_MAX;
    }

    size_t str_len = strlen(str);
    if (str_len == 0 || str_len > buffer_len) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i <= buffer_len - str_len; i++) {
        if (memcmp(buffer + i, str, str_len) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

/**
 * @brief Wait for data to become available
 * @param timeout_ms Timeout in milliseconds
 * @return UART_SUCCESS if data available, UART_ERROR_TIMEOUT otherwise
 */
static UART_ErrorTypeDef WaitForData(uint32_t timeout_ms)
{
    ResetTimeout();

    while (UART_Available() == 0) {
        if (IsTimeOutExpired(timeout_ms)) {
            return UART_ERROR_TIMEOUT;
        }
    }

    return UART_SUCCESS;
}
