/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Simplified Ardu-Stim serial communications
 * 
 * Commands:
 *   R<value> - Set RPM (e.g., R2500)
 *   O<value> - Set crank offset in degrees (e.g., O15)
 *   T[0|1] - Stop or start wheels (e.g., T0, T1, T)
 *   I[0|1] - Toggle output invert (e.g., I0, I1, I)
 *   H - Print help
 *   S - Print current status
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
#include "comms.h"

//! Initializes the serial port
void serialSetup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== Ardu-Stim Simplified (ESP8266 + Mitsubishi 6G72) ===");
  Serial.println("Commands:");
  Serial.println("  R<value>  - Set RPM (300-8000 RPM)");
  Serial.println("  O<value>  - Set crank offset (0-359 degrees)");
  Serial.println("  T[0|1]    - Stop (T0) or start/toggle (T1 or T) wheels");
  Serial.println("  I[0|1]    - Disable (I0) or enable/toggle (I1 or I) output invert");
  Serial.println("  S         - Print current status");
  Serial.println("  H         - Print this help");
  Serial.println("================================================\n");
  printStatus();
}

//! Simple command parser - reads commands from serial
void commandParser()
{
  if (Serial.available() > 0)
  {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove any trailing \r
    if (input.length() > 0)
    {
      // Echo the input for visibility
      Serial.println(input);
      
      char command = input[0];
      String inputValue = input.substring(1);
      
      switch (command)
      {
        case 'R':
        case 'r':
          if (inputValue.length() > 0)
          {
            uint16_t newRpm = inputValue.toInt();
            if (newRpm >= MIN_RPM && newRpm <= MAX_RPM)
            {
              config.rpm = newRpm;
              currentStatus.rpm = newRpm;
              setRPM(newRpm);
              Serial.print("RPM set to: ");
              Serial.println(newRpm);
              printStatus();
            }
            else
            {
              Serial.print("ERROR: RPM must be between ");
              Serial.print(MIN_RPM);
              Serial.print(" and ");
              Serial.println(MAX_RPM);
            }
          }
          break;

        case 'O':
        case 'o':
          if (inputValue.length() > 0)
          {
            int16_t newOffset = inputValue.toInt();
            // Allow negative or positive offsets, normalize to 0-359
            if (newOffset < 0)
            {
              newOffset = 360 + (newOffset % 360);
            }
            newOffset = newOffset % 360;
            
            config.crank_offset = newOffset;
            currentStatus.crank_offset = newOffset;
            applyOffset(newOffset);
            Serial.print("Crank offset set to: ");
            Serial.print(newOffset);
            Serial.println(" degrees");
            printStatus();
          }
          break;

        case 'T':
        case 't':
        {
          bool newState;
          if (inputValue.length() > 0)
          {
            newState = (inputValue.toInt() != 0);
          }
          else
          {
            newState = !currentStatus.spinning;
          }

          setWheelSpinEnabled(newState);
          Serial.print("Wheel spinning ");
          Serial.println(newState ? "ENABLED" : "DISABLED");
          printStatus();
        }
          break;

        case 'I':
        case 'i':
        {
          bool newState;
          if (inputValue.length() > 0)
          {
            newState = (inputValue.toInt() != 0);
          }
          else
          {
            newState = !output_invert;
          }

          output_invert = newState;
          Serial.print("Output invert ");
          Serial.println(newState ? "ENABLED" : "DISABLED");
          printStatus();
        }
          break;

        case 'S':
        case 's':
          printStatus();
          break;

        case 'H':
        case 'h':
          Serial.println("\n=== Help ===");
          Serial.println("Commands:");
          Serial.println("  R<value>  - Set RPM (300-8000 RPM)");
          Serial.println("  O<value>  - Set crank offset (0-359 degrees)");
          Serial.println("  T[0|1]    - Stop (T0) or start/toggle (T1 or T) wheels");
          Serial.println("  I[0|1]    - Disable (I0) or enable/toggle (I1 or I) output invert");
          Serial.println("  S         - Print current status");
          Serial.println("  H         - Print this help\n");
          break;

        default:
          // Ignore unknown commands
          break;
      }
    }
  }
}

//! Print current status to serial
void printStatus()
{
  Serial.print("Current Status: RPM=");
  Serial.print(currentStatus.rpm);
  Serial.print(" | Offset=");
  Serial.print(currentStatus.crank_offset);
  Serial.print(" deg");
  Serial.print(" | Spinning=");
  Serial.print(currentStatus.spinning ? "ON" : "OFF");
  Serial.print(" | Invert=");
  Serial.println(output_invert ? "ON" : "OFF");
}