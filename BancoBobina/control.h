#pragma once
#include <stdint.h>

// Orden exacto del archivo PWM_Metatron_Para_Placa_V2_2_0.ino.
constexpr int pwmPins[18] = {13,14,26,33,22,19,5,16,2,27,12,25,32,23,18,17,4,15};
constexpr uint32_t MIN_PERIOD_US = 4000;     // 250 Hz: limite del software, no de la bobina.
constexpr uint32_t MAX_PERIOD_US = 1000000;  // 1 Hz.
constexpr uint32_t MIN_TON_US = 100;
constexpr uint32_t MAX_TON_US = 5000;
constexpr uint32_t MIN_GAP_US = 100;
constexpr uint32_t ALL_PINS_MASK = (1UL << 18) - 1;
struct ControlSettings { uint32_t periodUs, tonUs, mask; };

inline const char* validateSettings(const ControlSettings& s) {
  if (s.periodUs < MIN_PERIOD_US || s.periodUs > MAX_PERIOD_US)
    return "Frecuencia fuera de 1 a 250 Hz (periodo de 4 a 1000 ms)";
  if (s.tonUs < MIN_TON_US || s.tonUs > MAX_TON_US)
    return "Pulso fuera de 0.1 a 5 ms";
  if (s.tonUs * 100UL > s.periodUs * 45UL)
    return "El ciclo de trabajo no puede superar 45% por grupo";
  if (s.tonUs + MIN_GAP_US > s.periodUs / 2)
    return "La pausa entre grupos debe ser al menos 0.1 ms";
  if (s.mask & ~ALL_PINS_MASK) return "Seleccion GPIO invalida";
  return nullptr;
}
