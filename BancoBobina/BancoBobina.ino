#include <WiFi.h>
#include <WebServer.h>
#include <driver/gptimer.h>
#include <soc/gpio_reg.h>
#include <esp_system.h>
#include "secrets.h"
#include "control.h"
#include "web_page.h"

IPAddress local_IP(192, 168, 0, 240), gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0), primaryDNS(192, 168, 0, 1);
const uint8_t accessPointBSSID[6] = {0xE4, 0xC0, 0xE2, 0x91, 0x5F, 0x32};
WebServer server(80);
gptimer_handle_t pulseTimer = nullptr;
portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;
ControlSettings settings = {10000, 3000, 0};
bool running = false, timerReady = false;
uint32_t generation = 0, bootId = 0, timerFaults = 0;
uint32_t groupLow[2] = {}, groupHigh[2] = {}, allLowMask = 0, allHighMask = 0;
uint32_t durations[4] = {};
uint64_t scheduledAt = 0;
uint8_t phase = 0;
enum Reason { BOOT, CONFIGURED, ACTIVE, STOPPED, WIFI_LOST, TIMER_FAULT };
Reason reason = BOOT;

// One write per GPIO bank; no loops, allocation or waiting in the interrupt.
void IRAM_ATTR allLow() {
  REG_WRITE(GPIO_OUT_W1TC_REG, allLowMask);
  REG_WRITE(GPIO_OUT1_W1TC_REG, allHighMask);
}

bool IRAM_ATTR armAfter(uint32_t duration) {
  uint64_t now;
  if (gptimer_get_raw_count(pulseTimer, &now) != ESP_OK) return false;
  scheduledAt = now + duration;
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = scheduledAt;
  return gptimer_set_alarm_action(pulseTimer, &alarm) == ESP_OK;
}

void IRAM_ATTR timerFault() {
  allLow(); running = false; reason = TIMER_FAULT;
  ++generation; ++timerFaults;
}

bool IRAM_ATTR onPulse(gptimer_handle_t, const gptimer_alarm_event_data_t* event, void*) {
  portENTER_CRITICAL_ISR(&controlMux);
  // Ignore an already pending interrupt from an earlier start/stop.
  if (running && event->alarm_value == scheduledAt) {
    if (event->count_value > scheduledAt + 500) {
      timerFault();
    } else {
      const unsigned group = phase / 2;
      if ((phase & 1) == 0) {
        REG_WRITE(GPIO_OUT_W1TS_REG, groupLow[group]);
        REG_WRITE(GPIO_OUT1_W1TS_REG, groupHigh[group]);
      } else allLow();
      // Schedule from now: never compress pulses to catch up after a delay.
      if (!armAfter(durations[phase])) timerFault();
      phase = (phase + 1) & 3;
    }
  }
  portEXIT_CRITICAL_ISR(&controlMux);
  return false;
}

// Caller holds controlMux. Invalidating the generation cancels stale web starts.
void stopLocked(Reason why) {
  running = false; ++generation; allLow(); reason = why;
  if (timerReady) gptimer_set_alarm_action(pulseTimer, nullptr);
}

void stopOutputs(Reason why) {
  portENTER_CRITICAL(&controlMux);
  stopLocked(why);
  portEXIT_CRITICAL(&controlMux);
}

const char* reasonText(Reason value) {
  switch (value) {
    case CONFIGURED: return "Configuracion aplicada: detenido";
    case ACTIVE: return "Generando PWM";
    case STOPPED: return "Detenido desde la web";
    case WIFI_LOST: return "Wi-Fi desconectado: detenido";
    case TIMER_FAULT: return "Temporizacion interrumpida: detenido";
    default: return "Arranque: salidas deshabilitadas";
  }
}

void sendStatus() {
  ControlSettings current;
  bool active;
  uint32_t revision, faults;
  Reason why;
  portENTER_CRITICAL(&controlMux);
  current = settings; active = running; revision = generation;
  why = reason; faults = timerFaults;
  portEXIT_CRITICAL(&controlMux);
  // Fixed buffer avoids repeated String growth and heap fragmentation.
  char json[800];
  snprintf(json, sizeof(json),
    "{\"periodUs\":%lu,\"tonUs\":%lu,\"mask\":%lu,\"running\":%s,"
    "\"revision\":%lu,\"bootId\":%lu,\"ready\":%s,\"reason\":\"%s\","
    "\"pins\":[13,14,26,33,22,19,5,16,2,27,12,25,32,23,18,17,4,15],"
    "\"uptimeMs\":%lu,\"freeHeap\":%lu,\"minHeap\":%lu,\"resetReason\":%d,\"timerFaults\":%lu,"
    "\"limits\":{\"minPeriodUs\":4000,\"maxPeriodUs\":1000000,\"minTonUs\":100,"
    "\"maxTonUs\":5000,\"maxDutyPercent\":45,\"minGapUs\":100}}",
    (unsigned long)current.periodUs, (unsigned long)current.tonUs, (unsigned long)current.mask,
    active ? "true" : "false", (unsigned long)revision, (unsigned long)bootId,
    timerReady ? "true" : "false", reasonText(why), (unsigned long)millis(),
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
    (int)esp_reset_reason(), (unsigned long)faults);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

bool readUnsigned(const char* name, uint32_t& result) {
  if (!server.hasArg(name)) return false;
  const String value = server.arg(name);
  if (value.length() == 0 || value.length() > 10) return false;
  result = 0;
  for (unsigned i = 0; i < value.length(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    const uint32_t digit = value[i] - '0';
    if (result > (UINT32_MAX - digit) / 10) return false;
    result = result * 10 + digit;
  }
  return true;
}

void configureOutputs(bool start) {
  ControlSettings next;
  uint32_t revision, boot;
  if (!readUnsigned("periodUs", next.periodUs) || !readUnsigned("tonUs", next.tonUs) ||
      !readUnsigned("mask", next.mask) || !readUnsigned("revision", revision) ||
      !readUnsigned("bootId", boot)) {
    server.send(400, "text/plain", "Faltan parametros enteros validos"); return;
  }
  const char* error = validateSettings(next);
  if (error) { server.send(400, "text/plain", error); return; }
  if (start && (!timerReady || WiFi.status() != WL_CONNECTED)) {
    server.send(503, "text/plain", "Generador no disponible"); return;
  }
  if (start && next.mask == 0) {
    server.send(400, "text/plain", "Selecciona al menos un GPIO"); return;
  }
  portENTER_CRITICAL(&controlMux);
  const bool stale = revision != generation || boot != bootId;
  if (!stale) {
    stopLocked(CONFIGURED);
    settings = next;
    if (start) {
      groupLow[0] = groupLow[1] = groupHigh[0] = groupHigh[1] = 0;
      for (unsigned i = 0; i < 18; ++i) if (next.mask & (1UL << i)) {
        const unsigned pin = pwmPins[i], group = i / 9;
        if (pin < 32) groupLow[group] |= 1UL << pin;
        else groupHigh[group] |= 1UL << (pin - 32);
      }
      durations[0] = durations[2] = next.tonUs;
      durations[1] = next.periodUs / 2 - next.tonUs;
      durations[3] = next.periodUs - next.periodUs / 2 - next.tonUs;
      phase = 0; running = true; reason = ACTIVE;
      if (!armAfter(100)) timerFault();
    }
  }
  portEXIT_CRITICAL(&controlMux);
  if (stale) { server.send(409, "text/plain", "Estado cambiado: actualiza y vuelve a intentar"); return; }
  sendStatus();
}

void setup() {
  Serial.begin(115200);
  bootId = esp_random();
  for (int pin : pwmPins) {
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
    if (pin < 32) allLowMask |= 1UL << pin; else allHighMask |= 1UL << (pin - 32);
  }
  allLow();
  gptimer_config_t config = {};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = 1000000;
  config.intr_priority = 2;
  gptimer_event_callbacks_t callbacks = {};
  callbacks.on_alarm = onPulse;
  timerReady = gptimer_new_timer(&config, &pulseTimer) == ESP_OK &&
    gptimer_register_event_callbacks(pulseTimer, &callbacks, nullptr) == ESP_OK &&
    gptimer_enable(pulseTimer) == ESP_OK && gptimer_start(pulseTimer) == ESP_OK;
  if (!timerReady) reason = TIMER_FAULT;
  Serial.printf("Reset=%d; timer=%d; firmware=timer-v2\n", esp_reset_reason(), timerReady);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("Error IP fija; sin conexion DHCP alternativa");
    while (true) delay(1000);
  }
  WiFi.begin(ssid, password, 1, accessPointBSSID);
  WiFi.setSleep(false);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", htmlPage);
  });
  server.on("/status", HTTP_GET, sendStatus);
  server.on("/configure", HTTP_POST, []() { configureOutputs(false); });
  server.on("/run", HTTP_POST, []() { configureOutputs(true); });
  server.on("/stop", HTTP_POST, []() { stopOutputs(STOPPED); sendStatus(); });
  server.begin();
  Serial.print("Control: http://"); Serial.println(local_IP);
}

void loop() {
  static unsigned long lastAttempt = 0;
  if (WiFi.status() == WL_CONNECTED) server.handleClient();
  else {
    portENTER_CRITICAL(&controlMux);
    if (running) stopLocked(WIFI_LOST);
    portEXIT_CRITICAL(&controlMux);
    if (millis() - lastAttempt >= 15000) { lastAttempt = millis(); WiFi.reconnect(); }
  }
  delay(2);
}
