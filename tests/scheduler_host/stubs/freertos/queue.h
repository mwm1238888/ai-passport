#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);
BaseType_t    xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, uint32_t xTicksToWait);
BaseType_t    xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait);