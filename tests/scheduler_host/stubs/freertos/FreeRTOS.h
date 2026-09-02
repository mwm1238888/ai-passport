#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int  BaseType_t;
typedef void *QueueHandle_t;
typedef void *TaskHandle_t;

#define pdTRUE   1
#define pdFALSE  0
#define pdPASS   1
#define pdFAIL   0
#define portMAX_DELAY 0xFFFFFFFFUL
#define pdMS_TO_TICKS(ms) ((ms) / 10)

typedef void (*TaskFunction_t)(void *);