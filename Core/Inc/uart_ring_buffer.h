/*
 * uart_ring_buffer.h
 *
 *  Created on: Sep 22, 2025
 *      Author: hendra-saputro
 */

#ifndef INC_UART_RING_BUFFER_H_
#define INC_UART_RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32l4xx_hal.h"  // Adjust according to your STM32 series

/**** Configuration ****/
#ifndef UART_BUFFER_SIZE
#define UART_BUFFER_SIZE 1024
#endif

/**** Type Definitions ****/
typedef enum {
    UART_SUCCESS = 0,
    UART_ERROR_TIMEOUT = -1,
    UART_ERROR_BUFFER_FULL = -2,
    UART_ERROR_BUFFER_EMPTY = -3,
    UART_ERROR_INVALID_PARAM = -4,
    UART_ERROR_NOT_FOUND = -5
} UART_ErrorTypeDef;

typedef struct {
    uint8_t buffer[UART_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer_TypeDef;

/**** Function Prototypes ****/

/**
 * @brief Initialize UART ring buffer system
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_RingBuff_Init(void);

/**
 * @brief Read a single character from RX buffer
 * @param c Pointer to store the character
 * @return UART_SUCCESS on success, UART_ERROR_BUFFER_EMPTY if no data
 */
UART_ErrorTypeDef UART_ReadChar(uint8_t *c);

/**
 * @brief Write a single character to TX buffer
 * @param c Character to write
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_WriteChar(uint8_t c);

/**
 * @brief Send null-terminated string via UART
 * @param str String to send
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_SendString(const char *str);

/**
 * @brief Check number of bytes available in RX buffer
 * @return Number of bytes available
 */
uint16_t UART_Available(void);

/**
 * @brief Peek at next character without removing it from buffer
 * @param c Pointer to store the character
 * @return UART_SUCCESS on success, UART_ERROR_BUFFER_EMPTY if no data
 */
UART_ErrorTypeDef UART_Peek(uint8_t *c);

/**
 * @brief Flush (clear) RX buffer
 */
void UART_FlushRX(void);

/**
 * @brief Wait for specific string to arrive with timeout
 * @param str String to wait for
 * @param timeout_ms Timeout in milliseconds
 * @return UART_SUCCESS if found, UART_ERROR_TIMEOUT if timeout occurred
 */
UART_ErrorTypeDef UART_WaitForString(const char *str, uint32_t timeout_ms);

/**
 * @brief Copy data from buffer until specific string is encountered
 * @param end_str String that marks the end of data to copy
 * @param buffer Destination buffer
 * @param buffer_size Size of destination buffer
 * @param timeout_ms Timeout in milliseconds
 * @return UART_SUCCESS if end string found, error code otherwise
 */
UART_ErrorTypeDef UART_CopyUntil(const char *end_str, char *buffer, size_t buffer_size, uint32_t timeout_ms);

/**
 * @brief Extract data between two delimiter strings
 * @param start_str Start delimiter string
 * @param end_str End delimiter string
 * @param source Source buffer to search in
 * @param dest Destination buffer for extracted data
 * @param dest_size Size of destination buffer
 * @return UART_SUCCESS on success, error code otherwise
 */
UART_ErrorTypeDef UART_ExtractBetween(const char *start_str, const char *end_str,
                                  const char *source, char *dest, size_t dest_size);

/**
 * @brief UART interrupt service routine handler
 * @note Call this function from your UART interrupt handler
 * @param huart UART handle pointer
 */
void UART_ISR_Handler(UART_HandleTypeDef *huart);

/**** Convenience Macros ****/
#define UART_DEFAULT_TIMEOUT 500

#endif /* INC_UART_RING_BUFFER_H_ */
