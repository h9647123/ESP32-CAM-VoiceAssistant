#pragma once
static void buttonSampler(void*) {
  Button* buttons[] = {&startButton, &nextButton, &prevButton};
  uint32_t pressGeneration[3] = {};
  for (;;) {
    for (uint8_t i = 0; i < 3; ++i) {
      bool click, held, repeat, released;
      const bool wasPressed = buttons[i]->pressed;
      buttons[i]->poll(click, held, repeat, released);
      if (!wasPressed && buttons[i]->pressed) pressGeneration[i] = buttonUiGeneration.load();
      if (click) ++buttonClicks[i];
      const uint8_t flags = (click ? 1 : 0) | (held ? 2 : 0) |
          (repeat ? 4 : 0) | (released ? 8 : 0);
      if (!flags || (repeat && uxQueueSpacesAvailable(buttonEvents) < 8)) continue;
      ButtonEvent event{pressGeneration[i], i, flags};
      if (xQueueSend(buttonEvents, &event, 0) != pdTRUE) {
        ButtonEvent discarded;
        xQueueReceive(buttonEvents, &discarded, 0);
        xQueueSend(buttonEvents, &event, 0);
        ++buttonOverflows;
      }
    }
    ++buttonSamples;
    vTaskDelay(max<TickType_t>(1, pdMS_TO_TICKS(5)));
  }
}

static void startButtonSampler() {
  buttonEvents = xQueueCreate(64, sizeof(ButtonEvent));
  if (buttonEvents && xTaskCreatePinnedToCore(buttonSampler, "buttons", 3072,
      nullptr, 2, &buttonSamplerTask, 0) != pdPASS) {
    vQueueDelete(buttonEvents); buttonEvents = nullptr;
  }
  Serial.printf("[BUTTONS] sampler=%d period_ms=%u\n", buttonEvents != nullptr,
      unsigned(max<TickType_t>(1, pdMS_TO_TICKS(5)) * portTICK_PERIOD_MS));
}

static void buttonSamplerStatus() {
  Serial.printf("[BUTTONS] active=%d samples=%lu clicks_A=%lu B=%lu C=%lu overflow=%lu pending=%u stack=%u\n",
      buttonEvents != nullptr, (unsigned long)buttonSamples.load(),
      (unsigned long)buttonClicks[0].load(), (unsigned long)buttonClicks[1].load(),
      (unsigned long)buttonClicks[2].load(), (unsigned long)buttonOverflows.load(),
      buttonEvents ? unsigned(uxQueueMessagesWaiting(buttonEvents)) : 0,
      buttonSamplerTask ? unsigned(uxTaskGetStackHighWaterMark(buttonSamplerTask)) : 0);
}
