#ifndef DEV_ACREL_DTSD1352_H
#define DEV_ACREL_DTSD1352_H

#include <stddef.h>
#include <stdint.h>

#include "app_message.h"

void Acrel_DTSD1352_Pack_EnergyTotal(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

void Acrel_DTSD1352_Pack_EnergyABC(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

void Acrel_DTSD1352_Pack_VI(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

void Acrel_DTSD1352_Pack_Power(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

void Acrel_DTSD1352_Pack_PF_Freq(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

#endif /* DEV_ACREL_DTSD1352_H */
