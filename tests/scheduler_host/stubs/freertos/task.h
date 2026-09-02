#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *pcName, uint32_t usStackDepth,
                       void *pvParameters, uint32_t uxPriority, TaskHandle_t *pxCreatedTask);
void vTaskDelay(uint32_t xTicksToDelay);