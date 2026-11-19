// Tagged serial logging over the USB UART (UART0).
#pragma once

#include <Arduino.h>

#define LOGF(tag, fmt, ...) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__)
