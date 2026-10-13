#pragma once

// Pin definition for AI Thinker ESP32-CAM module
// This is the standard ESP32-CAM board shipped with the ESP32-CAM-MB development board.
#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27

  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22

  // Onboard Peripherals
  #define FLASH_LED_PIN      4   // Active HIGH onboard high-power flash LED
  #define STATUS_LED_PIN    33   // Active LOW small red status LED
#else
  #error "Camera model not defined! Please select CAMERA_MODEL_AI_THINKER."
#endif
