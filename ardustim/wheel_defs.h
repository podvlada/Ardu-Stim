/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Simplified wheel definitions - Mitsubishi 6G72 only
 * 
 * copyright 2014 David J. Andruczyk
 * Modified for ESP8266 + single wheel simplification
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
#ifndef __WHEEL_DEFS_H__
#define __WHEEL_DEFS_H__

#include <pgmspace.h>

/* Mitsubishi 6G72 - DOHC CAS and TCDS wheel pattern */
typedef enum {
  SIX_G_SEVENTY_TWO_WITH_CAM = 0,
  MAX_WHEELS = 1
} WheelType;

/* Friendly name for the wheel */
const char six_g_seventy_two_with_cam_friendly_name[] PROGMEM = "Mitsubishi 6g72 with cam";

/* Mitsubishi 6g72 crank/cam pattern */
const unsigned char six_g_seventy_two_with_cam[] PROGMEM = 
  { /* Mitsubishi 6g72 */
    /* Crank signal's are 50 deg wide, and one per cylinder
     * Cam signals have 3 40 deg wide teeth and one 85 deg wide tooth
     * Counting from TDC#1
     * Crank: 40 deg high, 70 deg low (repeats whole cycle)
     * Cam: 70 deg high, 80 deg low, 40 deg high, 150 deg low,
     * 40 deg high, 130 deg low, 40 deg high, 155 deg low 
     */
    2,2,2,2,2,2,2,3,3,3,
    3,3,3,3,3,3,3,2,2,2,
    2,2,2,2,0,0,0,0,0,0,
    0,1,1,1,1,1,3,3,3,3,
    3,2,2,2,2,2,2,2,0,0,
    0,0,0,0,0,1,1,1,1,1,
    1,1,1,1,1,0,0,0,0,0,
    0,0,2,2,2,2,2,2,2,3,
    3,3,3,3,1,1,1,1,1,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,1,1,1,1,1,3,3,
    3,3,3,2,2,2,2,2,2,2,
    0,0,0,0,0,0,0,1,1,1,
    1,1,1,1,1,1,1,0,0,0,
    0,0,0,0
  };

#endif
