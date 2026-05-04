/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Arbritrary wheel pattern generator
 *
 * copyright 2014 David J. Andruczyk
 * 
 * Ardu-Stim software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ArduStim software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with any ArduStim software.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "globals.h"
#include "ardustim.h"
#include "enums.h"
#include "comms.h"
// #include "storage.h"
#include "wheel_defs.h"
#include <EEPROM.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

#if !defined(ESP8266)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifndef TIM_DIV1
#define TIM_DIV1 0
#endif
#ifndef TIM_LOOP
#define TIM_LOOP 0
#endif

static void (*timer1Handler)() = NULL;

void timer1_disable()
{
  TIMSK1 &= ~(1 << OCIE1A);
}

void timer1_isr_init()
{
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 &= ~(1 << OCIE1A);
  TIFR1 |= (1 << OCF1A);
}

void timer1_attachInterrupt(void (*f)())
{
  timer1Handler = f;
}

void timer1_write(uint32_t value)
{
  uint32_t ticks = value / 5;
  if (ticks < 1)
  {
    ticks = 1;
  }
  if (ticks > 0xFFFF)
  {
    ticks = 0xFFFF;
  }
  OCR1A = (uint16_t)ticks;
}

void timer1_enable(int div, int loop, bool enable)
{
  if (enable)
  {
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
  }
  else
  {
    TIMSK1 &= ~(1 << OCIE1A);
  }
}

ISR(TIMER1_COMPA_vect)
{
  if (timer1Handler)
  {
    timer1Handler();
  }
}
#endif

struct configTable config;
struct status currentStatus;

/* Sensistive stuff used in ISR's */
volatile uint16_t adc0; /* POT RPM */
volatile uint16_t adc1; /* Pot Wheel select */
/* Setting rpm to any value over 0 will enabled sweeping by default */
/* Stuff for handling prescaler changes (small tooth wheels are low RPM) */
volatile uint8_t analog_port = 0;
volatile bool adc0_read_complete = false;
volatile bool adc1_read_complete = false;
volatile bool reset_prescaler = false;
volatile uint8_t output_invert_mask = 0x00; /* Selective invert: bit 0=crank, bit 1=cam input polarity */
volatile uint8_t output_enable_mask = ENABLE_CRANK_BIT; /* Only crank output enabled by default */
volatile uint8_t prescaler_bits = 0;
volatile uint8_t last_prescaler_bits = 0;
volatile uint16_t new_OCR1A = 5000; /* sane default */
volatile uint16_t edge_counter = 0;
volatile uint16_t crank_offset_edges = 0;
volatile uint32_t cycleStartTime = micros();
volatile uint32_t cycleDuration = 0;
uint32_t analog_last_read_ms = 0;

#define CAM_SYNC_TRANSITIONS 10
volatile uint16_t camTransitionIndex[CAM_SYNC_TRANSITIONS];
volatile uint8_t camTransitionEdgeCount[CAM_SYNC_TRANSITIONS];
volatile uint8_t camTransitionState[CAM_SYNC_TRANSITIONS];
volatile int8_t lastCamTransitionIndex = -1;
volatile bool camPhaseSynced = false;
volatile uint32_t camLastEdgeTime = 0;
volatile uint32_t camEdgeIntervals[CAM_SYNC_TRANSITIONS];
volatile uint8_t camEdgeIntervalIndex = 0;
volatile uint8_t camEdgeIntervalCount = 0;
volatile bool camEdgeDetected = false;
volatile uint8_t camInputState = 0;
#define CAM_SIGNAL_TIMEOUT_MS 500  // Stop output if no cam edges for 500ms
uint32_t camLastEdgeDetectedMs = 0;

/* Less sensitive globals */
uint8_t bitshift = 0;

wheels Wheels[MAX_WHEELS] = {
  { six_g_seventy_two_with_cam_friendly_name, six_g_seventy_two_with_cam, 0.6, 144, 720 },
};

/* Initialization */
void initializeCamSync()
{
  uint16_t edges = Wheels[0].wheel_max_edges;
  uint8_t prevCam = pgm_read_byte(&Wheels[0].edge_states_ptr[0]) & 0x02;
  uint8_t index = 0;

  for (uint16_t i = 1; i < edges; i++) {
    uint8_t cam = pgm_read_byte(&Wheels[0].edge_states_ptr[i]) & 0x02;
    if (cam != prevCam && index < CAM_SYNC_TRANSITIONS) {
      camTransitionIndex[index] = i;
      camTransitionState[index] = (cam ? 1 : 0);
      prevCam = cam;
      index++;
    }
  }

  if (index == CAM_SYNC_TRANSITIONS) {
    for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++) {
      uint16_t nextIndex = camTransitionIndex[(i + 1) % CAM_SYNC_TRANSITIONS];
      if (i == CAM_SYNC_TRANSITIONS - 1) {
        nextIndex += edges;
      }
      camTransitionEdgeCount[i] = (uint8_t)(nextIndex - camTransitionIndex[i]);
    }
  }
}

IRAM_ATTR void timer1_isr();
IRAM_ATTR void camInputISR();

void setup() {
  serialSetup();

  // Initialize config
  config.rpm = 0;
  config.crank_offset = 0;
  currentStatus.rpm = 0;
  currentStatus.crank_offset = 0;
  currentStatus.spinning = false;

  pinMode(PRIMARY_OUTPUT_PIN, OUTPUT);
  pinMode(CAM_INPUT_PIN, INPUT);
  pinMode(TERTIARY_OUTPUT_PIN, OUTPUT);
  pinMode(KNOCK_OUTPUT_PIN, OUTPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(CAM_INPUT_PIN), camInputISR, CHANGE);

  initializeCamSync();

  timer1_disable();
  timer1_isr_init();
  timer1_attachInterrupt(timer1_isr);
  timer1_write((uint32_t)new_OCR1A * 160); // Scale from 500kHz constant to 80MHz timer

  setRPM(MIN_RPM);
}

IRAM_ATTR void writeOutputPattern(uint8_t pattern)
{
  // Always ensure output is silent if not spinning
  if (!currentStatus.spinning)
  {
    pattern = 0;
  }

  // Apply enable mask - disable crank output when turned off
  if (!(output_enable_mask & ENABLE_CRANK_BIT))
  {
    pattern &= ~0x01;  // clear crank bit (bit 0)
  }

  // Selectively invert crank bit based on mask
  if (output_invert_mask & INVERT_CRANK_BIT)
  {
    pattern ^= 0x01;  // invert crank (bit 0)
  }

  digitalWrite(PRIMARY_OUTPUT_PIN, (pattern & 0x01) ? HIGH : LOW);
  digitalWrite(ONBOARD_LED_PIN, (pattern & 0x01) ? HIGH : LOW);
}

IRAM_ATTR void timer1_isr()
{
  uint16_t crank_index = edge_counter + crank_offset_edges;
  if (crank_index >= Wheels[0].wheel_max_edges)
  {
    crank_index -= Wheels[0].wheel_max_edges;
  }

  uint8_t crankState = pgm_read_byte(&Wheels[0].edge_states_ptr[crank_index]) & 0x01;
  writeOutputPattern(crankState);

  edge_counter++;
  if (edge_counter == Wheels[0].wheel_max_edges)
  {
    edge_counter = 0;
    cycleDuration = micros() - cycleStartTime;
    cycleStartTime = micros();
  }

  timer1_write((uint32_t)new_OCR1A * 160);
}

IRAM_ATTR void camInputISR()
{
  uint32_t now = micros();
  uint8_t rawState = digitalRead(CAM_INPUT_PIN);
  uint8_t state = (rawState ^ ((output_invert_mask & INVERT_CAM_BIT) ? 1 : 0)) ? 1 : 0;

  if (camLastEdgeTime != 0)
  {
    camEdgeIntervals[camEdgeIntervalIndex] = now - camLastEdgeTime;
    camEdgeIntervalIndex = (camEdgeIntervalIndex + 1) % CAM_SYNC_TRANSITIONS;
    if (camEdgeIntervalCount < CAM_SYNC_TRANSITIONS)
    {
      camEdgeIntervalCount++;
    }
  }

  camLastEdgeTime = now;
  camInputState = state;
  camEdgeDetected = true;
}

static int8_t findCamPhaseMatch(const uint32_t intervals[], uint8_t currentState)
{
  uint32_t totalTime = 0;
  for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++)
  {
    totalTime += intervals[i];
  }

  if (totalTime == 0)
  {
    return -1;
  }

  float measured[CAM_SYNC_TRANSITIONS];
  for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++)
  {
    measured[i] = (float)intervals[i] / (float)totalTime;
  }

  float bestError = 1e6;
  int8_t bestTransition = -1;

  for (uint8_t start = 0; start < CAM_SYNC_TRANSITIONS; start++)
  {
    float error = 0.0f;
    for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++)
    {
      uint8_t idx = (start + i) % CAM_SYNC_TRANSITIONS;
      float expected = (float)camTransitionEdgeCount[idx] / (float)Wheels[0].wheel_max_edges;
      float diff = measured[i] - expected;
      if (diff < 0.0f)
      {
        diff = -diff;
      }
      error += diff;
    }

    uint8_t transitionIndex = (start + CAM_SYNC_TRANSITIONS - 1) % CAM_SYNC_TRANSITIONS;
    if (camTransitionState[transitionIndex] != currentState)
    {
      error += 1.0f;
    }

    if (error < bestError)
    {
      bestError = error;
      bestTransition = transitionIndex;
    }
  }

  if (bestError > 0.8f)
  {
    return -1;
  }

  return bestTransition;
}

void handleCamInputEvents()
{
  // Check for cam signal timeout
  uint32_t nowMs = millis();
  if (currentStatus.spinning && (nowMs - camLastEdgeDetectedMs) > CAM_SIGNAL_TIMEOUT_MS)
  {
    // Cam signal lost - stop output
    setWheelSpinEnabled(false);
    camPhaseSynced = false;
    lastCamTransitionIndex = -1;
    Serial.println("CAM signal lost - output disabled");
  }

  if (!camEdgeDetected)
  {
    return;
  }

  noInterrupts();
  bool eventPending = camEdgeDetected;
  camEdgeDetected = false;
  uint32_t localIntervals[CAM_SYNC_TRANSITIONS];
  uint8_t localIndex = camEdgeIntervalIndex;
  uint8_t localCount = camEdgeIntervalCount;
  uint8_t localState = camInputState;

  for (uint8_t i = 0; i < localCount; i++)
  {
    localIntervals[i] = camEdgeIntervals[i];
  }
  interrupts();

  if (!eventPending)
  {
    return;
  }

  camLastEdgeDetectedMs = nowMs;  // Record edge detection time

  if (localCount == CAM_SYNC_TRANSITIONS)
  {
    uint32_t ordered[CAM_SYNC_TRANSITIONS];
    for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++)
    {
      ordered[i] = localIntervals[(localIndex + i) % CAM_SYNC_TRANSITIONS];
    }

    int8_t matchedTransition = findCamPhaseMatch(ordered, localState);
    if (matchedTransition >= 0)
    {
      noInterrupts();
      lastCamTransitionIndex = matchedTransition;
      edge_counter = camTransitionIndex[matchedTransition];
      camPhaseSynced = true;
      interrupts();
    }

    uint32_t totalTime = 0;
    for (uint8_t i = 0; i < CAM_SYNC_TRANSITIONS; i++)
    {
      totalTime += ordered[i];
    }

    if (totalTime > 0)
    {
      uint16_t measuredRpm = (uint16_t)(60000000UL / totalTime);
      if (measuredRpm < MIN_RPM)
      {
        measuredRpm = MIN_RPM;
      }
      else if (measuredRpm > MAX_RPM)
      {
        measuredRpm = MAX_RPM;
      }
      currentStatus.rpm = measuredRpm;
      setRPM(measuredRpm);
      if (!currentStatus.spinning)
      {
        setWheelSpinEnabled(true);
      }
    }
  }
  else if (camPhaseSynced && lastCamTransitionIndex >= 0)
  {
    noInterrupts();
    lastCamTransitionIndex = (lastCamTransitionIndex + 1) % CAM_SYNC_TRANSITIONS;
    edge_counter = camTransitionIndex[lastCamTransitionIndex];
    interrupts();
  }
}

void loop() 
{
  // Handle serial commands
  if (Serial.available() > 0)
  {
    commandParser();
  }

  handleCamInputEvents();
}

//! Set RPM
void setRPM(uint16_t newRPM)
{
  if (newRPM < MIN_RPM) { return; }
  if (newRPM > MAX_RPM) { return; }

  currentStatus.rpm = newRPM;
  
  // Calculate timer value for ESP8266
  uint32_t tmp = (uint32_t)(500000.0 / (Wheels[0].rpm_scaler * (float)newRPM));
  if (tmp < 1) { tmp = 1; }
  new_OCR1A = (uint16_t)tmp;
}

void setWheelSpinEnabled(bool enabled)
{
  if (currentStatus.spinning == enabled) { return; }

  currentStatus.spinning = enabled;
  if (enabled)
  {
    cycleStartTime = micros();
    timer1_write((uint32_t)new_OCR1A * 160);
    timer1_enable(TIM_DIV1, TIM_LOOP, true);
  }
  else
  {
    timer1_disable();
    writeOutputPattern(0);
  }
}

//! Apply crank offset relative to cam by storing the crank shift in wheel-edge units
void applyOffset(int16_t offset_degrees)
{
  // 6G72 has 144 edges per 720 degrees = 0.2 degrees per edge = 5 edges per degree
  int32_t offset_edges = (int32_t)offset_degrees * 5;
  
  // Normalize to valid range
  while (offset_edges < 0) { offset_edges += Wheels[0].wheel_max_edges; }
  offset_edges = offset_edges % Wheels[0].wheel_max_edges;
  
  crank_offset_edges = (uint16_t)offset_edges;
}
