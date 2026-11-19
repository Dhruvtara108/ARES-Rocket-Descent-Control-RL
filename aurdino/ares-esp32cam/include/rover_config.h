// ARES Rover ESP32-CAM configuration.
// Every tunable value lives here. Values marked VERIFY must be checked against
// the physical rover before motors are connected.
#pragma once

#include <stdint.h>

namespace cfg {

// ---------------------------------------------------------------------------
// Camera board: AI-Thinker ESP32-CAM pin map (VERIFY).
// The team's board photo shows the AI-Thinker header layout
// (5V GND IO12 IO13 IO15 IO14 IO2 IO4 / 3V3 IO16 IO0 GND VCC U0R U0T GND).
// These are the standard esp32-camera AI-Thinker assignments. If your board is
// a different model, replace this block with that board's map.
// ---------------------------------------------------------------------------
constexpr int CAM_PIN_PWDN = 32;
constexpr int CAM_PIN_RESET = -1;
constexpr int CAM_PIN_XCLK = 0;
constexpr int CAM_PIN_SIOD = 26;
constexpr int CAM_PIN_SIOC = 27;
constexpr int CAM_PIN_D7 = 35;  // Y9
constexpr int CAM_PIN_D6 = 34;  // Y8
constexpr int CAM_PIN_D5 = 39;  // Y7
constexpr int CAM_PIN_D4 = 36;  // Y6
constexpr int CAM_PIN_D3 = 21;  // Y5
constexpr int CAM_PIN_D2 = 19;  // Y4
constexpr int CAM_PIN_D1 = 18;  // Y3
constexpr int CAM_PIN_D0 = 5;   // Y2
constexpr int CAM_PIN_VSYNC = 25;
constexpr int CAM_PIN_HREF = 23;
constexpr int CAM_PIN_PCLK = 22;
constexpr uint32_t CAM_XCLK_HZ = 20000000;

// Expected sensor. OV3660 PID is 0x3660 (esp32-camera). The firmware logs the
// detected PID and keeps running with a warning if it differs.
constexpr uint16_t CAM_EXPECTED_PID = 0x3660;

// Capture 160x120 (QQVGA) grayscale directly: no JPEG decode on the MCU.
constexpr int CAM_WIDTH = 160;
constexpr int CAM_HEIGHT = 120;
constexpr uint32_t FRAME_PERIOD_MS = 100;        // ~10 Hz processing target
constexpr uint8_t CAM_MAX_CONSECUTIVE_FAILS = 5; // then camera is marked failed
constexpr uint32_t CAM_REINIT_BACKOFF_MS = 3000;

// ---------------------------------------------------------------------------
// UART to Arduino Nano (VERIFY - NOT taken from the repository).
// The repository has no existing ESP32 <-> Nano wiring or protocol. GPIO13/14
// are free on the AI-Thinker board when the microSD card is NOT used.
// Avoid GPIO0/2/12/15 (boot strapping), GPIO4 (flash LED), GPIO16 (PSRAM),
// GPIO1/3 (USB programming and logs).
//
// Safety interlock: until UART_WIRING_CONFIRMED is set to true, the UART is
// never started, the link can never come up, and the rover can never move.
// ---------------------------------------------------------------------------
constexpr bool UART_WIRING_CONFIRMED = false;
constexpr int NANO_UART_RX_PIN = 13;  // from Nano TX via 5V->3.3V divider/shifter
constexpr int NANO_UART_TX_PIN = 14;  // to Nano RX (3.3V is a valid HIGH for the Nano)
constexpr uint32_t NANO_UART_BAUD = 9600; // matches ares-nano monitor_speed; SoftwareSerial-safe

constexpr uint32_t HEARTBEAT_MS = 200;      // command refresh period
constexpr uint32_t MIN_SEND_GAP_MS = 50;    // never send faster than this
constexpr uint32_t LINK_TIMEOUT_MS = 800;   // no ACK for this long => link down
constexpr uint32_t NANO_TIMEOUT_MS = 500;   // Nano must stop motors after this (Nano side)

// ---------------------------------------------------------------------------
// Motion limits (PWM 0-255 sent to the Nano, which applies its own clamp).
// ---------------------------------------------------------------------------
constexpr uint8_t SPEED_FORWARD = 110;
constexpr uint8_t SPEED_TURN = 120;
constexpr uint8_t SPEED_MAX = 150;

// ---------------------------------------------------------------------------
// Navigation timing.
// ---------------------------------------------------------------------------
constexpr uint32_t FRAME_MAX_AGE_MS = 300;    // older detections count as stale
constexpr uint32_t SETTLE_MS = 400;           // stop-and-look before any move
constexpr uint32_t FORWARD_MAX_MS = 1500;     // longest continuous forward run
constexpr uint32_t FORWARD_PAUSE_MS = 300;    // mandatory pause after a run
constexpr uint32_t TURN_DURATION_MS = 350;    // one timed turn step
constexpr uint32_t TURN_COOLDOWN_MS = 2000;   // no direction reversal inside this
constexpr uint8_t MAX_CONSECUTIVE_TURNS = 8;  // then stop: no clear heading found
constexpr uint32_t STOP_RETRY_MS = 2000;      // wait before re-checking after a stop
constexpr float TURN_TIE_MARGIN = 0.05f;      // side scores closer than this are a tie

// ---------------------------------------------------------------------------
// Vision (operates on an 80x60 image downsampled from 160x120).
// ---------------------------------------------------------------------------
constexpr int PROC_W = 80;
constexpr int PROC_H = 60;
constexpr int ROI_TOP = 24;                    // analyse rows 24..59 (nearer ground)
constexpr int REF_X0 = 28, REF_X1 = 51;        // floor reference patch (inclusive)
constexpr int REF_Y0 = 50, REF_Y1 = 59;
constexpr uint8_t WARMUP_FRAMES = 10;          // frames to learn the floor model
constexpr float REF_MAX_STD = 22.0f;           // reference patch must be this uniform
constexpr float FLOOR_K_SIGMA = 3.0f;          // pixel differs from floor if > k*std
constexpr float FLOOR_MIN_DELTA = 18.0f;       // ... and by at least this much
constexpr float FLOOR_DRIFT_MAX = 30.0f;       // ref patch far from model => blocked
constexpr float FLOOR_MODEL_ALPHA = 0.05f;     // slow floor model adaptation
constexpr int EDGE_THRESHOLD = 60;             // |gx|+|gy| Sobel edge threshold
constexpr float EDGE_WEIGHT = 0.5f;            // edge-only pixel contributes half
constexpr float SCORE_EMA_ALPHA = 0.4f;        // temporal smoothing of region scores
constexpr float BLOCK_ON = 0.30f;              // filtered score to declare blocked
constexpr float BLOCK_OFF = 0.18f;             // filtered score to declare clear
constexpr uint8_t CONFIRM_FRAMES = 2;          // frames beyond threshold to switch
constexpr float DARK_MEAN = 20.0f;             // image too dark => uncertain
constexpr float BRIGHT_MEAN = 235.0f;          // image saturated => uncertain

// ---------------------------------------------------------------------------
// Diagnostics.
// ---------------------------------------------------------------------------
constexpr uint32_t USB_LOG_BAUD = 115200;
constexpr uint32_t VISION_LOG_PERIOD_MS = 1000;
constexpr uint32_t HEAP_LOG_PERIOD_MS = 5000;

}  // namespace cfg
