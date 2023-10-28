#pragma once
#include "SnapProcessor.h"

class SnapProcessorTasks;
static SnapProcessorTasks *selfSnapProcessorTasks = nullptr;

class SnapProcessorTasks : public SnapProcessor {
public:
  SnapProcessorTasks(SnapOutput &output) : SnapProcessor(output) {
    selfSnapProcessorTasks = this;
  }

  bool begin() override {
    bool result = SnapProcessor::begin();
    if (http_get_task_handle == nullptr) {
      result = xTaskCreatePinnedToCore(
          &http_get_task, "HTTP", CONFIG_TASK_STACK_HTTP, NULL,
          CONFIG_TASK_PRIORITY, &http_get_task_handle, CONFIG_TASK_CORE);
    }
    return result;
  }

  void end() {
    if (http_get_task_handle != nullptr)
      vTaskDelete(http_get_task_handle);
    SnapProcessor::end();
  }

protected:
  xTaskHandle http_get_task_handle = nullptr;

  // do nothing!
  void doLoop() override {}

  /// FreeRTOS Task Handler
  static void http_get_task(void *pvParameters) { selfSnapProcessorTasks->doLoop(); }
};