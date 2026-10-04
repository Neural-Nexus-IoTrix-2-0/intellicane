#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// Pin Definitions (ESP32 DevKit V1)
// ============================================================================

// I2C Bus Pins (Shared between VL53L1X, MPU6050, and optional VL53L0X)
constexpr uint8_t PIN_I2C_SDA             = 21;
constexpr uint8_t PIN_I2C_SCL             = 22;

// Haptic & Audio Alert Actuators
constexpr uint8_t PIN_VIBRATION_PWM       = 25; // LEDC PWM to Transistor/MOSFET driver
constexpr uint8_t PIN_BUZZER              = 26; // Piezo buzzer alarm pin
constexpr uint8_t PIN_LED_STATUS          = 2;  // Onboard status LED
constexpr uint8_t PIN_BUTTON_SOS          = 27; // SOS / Reset button (active LOW)

// Downward Sensor Pins
constexpr uint8_t PIN_US_TRIG             = 18; // Ultrasonic trigger pin
constexpr uint8_t PIN_US_ECHO             = 5;  // Ultrasonic echo pin (via 5V->3.3V divider)
constexpr uint8_t PIN_TOF_DOWN_XSHUT      = 19; // XSHUT pin for secondary ToF (VL53L0X)

// ============================================================================
// Sensor Type Selection
// ============================================================================
#define SENSOR_TYPE_ULTRASONIC 1
#define SENSOR_TYPE_VL53L0X    2

// Set active downward sensor type:
#ifndef DOWNWARD_SENSOR_TYPE
#define DOWNWARD_SENSOR_TYPE SENSOR_TYPE_ULTRASONIC
#endif

// ============================================================================
// Forward Obstacle Detection Parameters (VL53L1X)
// ============================================================================
// Distance boundaries in Centimeters
constexpr float FORWARD_MAX_ALERT_DIST_CM    = 150.0f; // Beyond this: motor OFF (clear)
constexpr float FORWARD_CAUTION_DIST_CM      = 80.0f;  // Gentle vibration begins
constexpr float FORWARD_CRITICAL_DIST_CM     = 30.0f;  // Maximum vibration + buzzer
constexpr float FORWARD_MIN_BLIND_DIST_CM    = 5.0f;   // Lower limit sensor saturation

// Haptic Vibration PWM Tuning (ESP32 LEDC 8-bit: 0 - 255)
constexpr uint8_t HAPTIC_PWM_CHANNEL         = 0;
constexpr uint32_t HAPTIC_PWM_FREQ           = 5000;   // 5 kHz PWM
constexpr uint8_t HAPTIC_PWM_RESOLUTION      = 8;      // 8-bit (0..255)
constexpr uint8_t HAPTIC_PWM_MIN_SPIN        = 85;     // Minimum duty cycle to overcome motor inertia
constexpr uint8_t HAPTIC_PWM_MAX             = 255;    // Max duty cycle (100%)

// ============================================================================
// Downward Drop-off Detection Parameters
// ============================================================================
// Nominal distance from angled downward sensor to ground when walking
constexpr float DOWNWARD_BASELINE_NOMINAL_CM = 40.0f;  
// Increase in distance above baseline to declare drop-off / hole / curb
constexpr float DROP_OFF_DELTA_THRESHOLD_CM  = 20.0f;  // e.g. reading > (40 + 20) = 60 cm
constexpr float DOWNWARD_MAX_VALID_DIST_CM   = 250.0f; // Max sensible ground reading

// Drop-off Haptic Signature: Double Rhythmic Burst (ms)
constexpr uint32_t DROP_OFF_PULSE_ON_1_MS    = 150;
constexpr uint32_t DROP_OFF_PULSE_OFF_1_MS   = 80;
constexpr uint32_t DROP_OFF_PULSE_ON_2_MS    = 150;
constexpr uint32_t DROP_OFF_CYCLE_PERIOD_MS  = 650;    // Total cycle before repetition

// ============================================================================
// Fall & Orientation Detection Parameters (MPU6050)
// ============================================================================
constexpr float FALL_TILT_CRITICAL_DEG       = 70.0f;  // Cane horizontal on ground
constexpr float FALL_FREEFALL_G_THRESH       = 0.40f;  // Freefall acceleration (< 0.4g)
constexpr float FALL_IMPACT_G_THRESH         = 2.50f;  // Impact acceleration spike (> 2.5g)
constexpr uint32_t FALL_IMMOBILITY_TIME_MS   = 3000;   // Inactivity period after impact before alarm

// ============================================================================
// Loop Update Intervals (Cooperative Scheduling in ms)
// ============================================================================
constexpr uint32_t INTERVAL_FORWARD_SENSOR_MS   = 30;  // ~33 Hz
constexpr uint32_t INTERVAL_DOWNWARD_SENSOR_MS  = 50;  // 20 Hz
constexpr uint32_t INTERVAL_MOTION_SENSOR_MS    = 25;  // 40 Hz
constexpr uint32_t INTERVAL_ACTUATOR_UPDATE_MS  = 10;  // 100 Hz
constexpr uint32_t INTERVAL_TELEMETRY_LOG_MS    = 100; // 10 Hz Serial streaming

#endif // CONFIG_H
