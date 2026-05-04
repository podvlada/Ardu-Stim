/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Simplified Ardu-Stim for ESP8266 + Mitsubishi 6G72 only
 * Serial console control: RPM and crank offset
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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "Arduino.h"
#include "wheel_defs.h"

// ESP8266 pin definitions
#define PRIMARY_OUTPUT_PIN D5
#define SECONDARY_OUTPUT_PIN D6
#define TERTIARY_OUTPUT_PIN D7
#define KNOCK_OUTPUT_PIN D8
#define ONBOARD_LED_PIN LED_BUILTIN

// Mitsubishi 6G72 configuration
#define MITSUBISHI_6G72_WHEEL_INDEX 0
#define MAX_CRANK_OFFSET 359  // Maximum crank offset in degrees
#define MIN_RPM 10
#define MAX_RPM 8000

// Output invert mask bits
#define INVERT_CRANK_BIT 0x01  // Bit 0: invert crank output
#define INVERT_CAM_BIT   0x02  // Bit 1: invert cam output

// Output enable mask bits
#define ENABLE_CRANK_BIT 0x01  // Bit 0: enable crank output
#define ENABLE_CAM_BIT   0x02  // Bit 1: enable cam output

// Simplified config for serial-controlled operation
struct configTable 
{
  uint16_t rpm;
  int16_t crank_offset;  // Crank offset in degrees (signed for flexibility)
} __attribute__ ((packed));
extern struct configTable config;

extern volatile uint8_t output_invert_mask;
extern volatile uint8_t output_enable_mask;

struct status 
{
  uint16_t rpm;
  int16_t crank_offset;
  bool spinning;
};
extern struct status currentStatus;
extern volatile bool output_invert;

/* Tie things wheel related into one nicer structure ... */
typedef struct _wheels wheels;
struct _wheels {
  const char *decoder_name;
  const unsigned char *edge_states_ptr;
  const float rpm_scaler;
  const uint16_t wheel_max_edges;
  const uint16_t wheel_degrees;
};

#endif
