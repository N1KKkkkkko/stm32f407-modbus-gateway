/***********************************************************************
 * Layer: Hardware Link / Modbus Transport
 * Responsibility: USART2 DMA, IDLE events, RX ring buffer, RS485 DE,
 *                 and byte-stream read/write/flush primitives.
 * Not responsible: frame validation, CRC, function codes or application data.
 ***********************************************************************/

#ifndef RS485_PORT_H
#define RS485_PORT_H

#include <stdint.h>

#include "cmsis_os.h"

void RS485_PortSetParserTask(osThreadId_t parser_task);
uint8_t RS485_PortInit(void);
uint8_t RS485_PortRecover(void);
void RS485_PortFlush(void);

int32_t RS485_PortRead(
    uint8_t *buffer,
    uint16_t count,
    int32_t byte_timeout_ms,
    void *argument
);

int32_t RS485_PortWrite(
    const uint8_t *buffer,
    uint16_t count,
    int32_t byte_timeout_ms,
    void *argument
);

#endif /* RS485_PORT_H */
