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
volatile uint8_t output_invert_mask = 0x00; /* Selective invert: bit 0=crank, bit 1=cam */
volatile uint8_t output_enable_mask = 0x03; /* Enable mask: bit 0=crank, bit 1=cam (both enabled by default) */
volatile uint8_t prescaler_bits = 0;
volatile uint8_t last_prescaler_bits = 0;
volatile uint16_t new_OCR1A = 5000; /* sane default */
volatile uint16_t edge_counter = 0;
volatile uint16_t crank_offset_edges = 0;
volatile uint32_t cycleStartTime = micros();
volatile   uint32_t cycleDuration = 0;
uint32_t analog_last_read_ms = 0;

/* Less sensitive globals */
uint8_t bitshift = 0;

wheels Wheels[MAX_WHEELS] = {
  { six_g_seventy_two_with_cam_friendly_name, six_g_seventy_two_with_cam, 0.6, 144, 720 },
};

/* Initialization */
void setup() {
  serialSetup();
  
  // Initialize config
  config.rpm = 300;
  config.crank_offset = 0;
  currentStatus.rpm = 300;
  currentStatus.crank_offset = 0;
  currentStatus.spinning = true;
  
  pinMode(PRIMARY_OUTPUT_PIN, OUTPUT);
  pinMode(SECONDARY_OUTPUT_PIN, OUTPUT);
  pinMode(TERTIARY_OUTPUT_PIN, OUTPUT);
  pinMode(KNOCK_OUTPUT_PIN, OUTPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);

  timer1_disable();
  timer1_isr_init();
  timer1_attachInterrupt(timer1_isr);
  timer1_write((uint32_t)new_OCR1A * 160); // Scale from 500kHz constant to 80MHz timer
  timer1_enable(TIM_DIV1, TIM_LOOP, true);

  // Set initial RPM
  setRPM(config.rpm);
}

ICACHE_RAM_ATTR void writeOutputPattern(uint8_t pattern)
{
  // Apply enable mask - disable outputs that are turned off
  if (!(output_enable_mask & ENABLE_CRANK_BIT))
  {
    pattern &= ~0x01;  // clear crank bit (bit 0)
  }
  if (!(output_enable_mask & ENABLE_CAM_BIT))
  {
    pattern &= ~0x02;  // clear cam bit (bit 1)
  }

  // Selectively invert bits based on mask
  if (output_invert_mask & INVERT_CRANK_BIT)
  {
    pattern ^= 0x01;  // invert crank (bit 0)
  }
  if (output_invert_mask & INVERT_CAM_BIT)
  {
    pattern ^= 0x02;  // invert cam (bit 1)
  }

  digitalWrite(PRIMARY_OUTPUT_PIN, (pattern & 0x01) ? HIGH : LOW);
  digitalWrite(SECONDARY_OUTPUT_PIN, (pattern & 0x02) ? HIGH : LOW);
  digitalWrite(TERTIARY_OUTPUT_PIN, (pattern & 0x04) ? HIGH : LOW);
  digitalWrite(ONBOARD_LED_PIN, (pattern & 0x02) ? LOW : HIGH);
}

ICACHE_RAM_ATTR void timer1_isr()
{
  uint16_t crank_index = edge_counter + crank_offset_edges;
  if (crank_index >= Wheels[0].wheel_max_edges)
  {
    crank_index -= Wheels[0].wheel_max_edges;
  }

  uint8_t camState = pgm_read_byte(&Wheels[0].edge_states_ptr[edge_counter]) & 0x02;
  uint8_t crankState = pgm_read_byte(&Wheels[0].edge_states_ptr[crank_index]) & 0x01;
  uint8_t outputState = camState | crankState;
  writeOutputPattern(outputState);

  edge_counter++;
  if (edge_counter == Wheels[0].wheel_max_edges)
  {
    edge_counter = 0;
    cycleDuration = micros() - cycleStartTime;
    cycleStartTime = micros();
  }

  timer1_write((uint32_t)new_OCR1A * 160);
}

void loop() 
{
  // Handle serial commands
  if(Serial.available() > 0) { 
    commandParser(); 
  }
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
