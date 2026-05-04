/* vim: set syntax=c expandtab sw=2 softtabstop=2 autoindent smartindent smarttab : */
/*
 * Simplified Ardu-Stim serial communications
 * 
 * Commands:
 *   O<value> - Set crank offset in degrees (e.g., O15)
 *   T[0|1] - Stop or start output (e.g., T0, T1, T)
 *   IK[0|1] - Invert crank output (e.g., IK0, IK1, IK)
 *   IM[0|1] - Invert cam input polarity (e.g., IM0, IM1, IM)
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
  Serial.println("  O<value>  - Set crank offset (0-359 degrees)");
  Serial.println("  T[0|1]    - Stop (T0) or start/toggle (T1 or T) output");
  Serial.println("  IK[0|1]   - Disable (IK0), enable (IK1), or toggle (IK) crank invert");
  Serial.println("  IM[0|1]   - Disable (IM0), enable (IM1), or toggle (IM) cam input invert");
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
          if (inputValue.length() < 1)
          {
            // No argument provided for I command
            Serial.println("ERROR: I command requires K or M (e.g., IK, IM)");
            break;
          }

          char invertTarget = inputValue[0];
          String invertValue = inputValue.substring(1);
          uint8_t bitMask = 0;
          const char* targetName = "";

          if (invertTarget == 'K' || invertTarget == 'k')
          {
            bitMask = INVERT_CRANK_BIT;
            targetName = "Crank";
          }
          else if (invertTarget == 'M' || invertTarget == 'm')
          {
            bitMask = INVERT_CAM_BIT;
            targetName = "Cam";
          }
          else
          {
            Serial.println("ERROR: Invalid invert target. Use K (crank) or M (cam)");
            break;
          }

          bool newState;
          if (invertValue.length() > 0)
          {
            newState = (invertValue.toInt() != 0);
          }
          else
          {
            // Toggle current state
            newState = !(output_invert_mask & bitMask);
          }

          if (newState)
          {
            output_invert_mask |= bitMask;  // Set bit
          }
          else
          {
            output_invert_mask &= ~bitMask; // Clear bit
          }

          Serial.print(targetName);
          Serial.print(" invert ");
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
          Serial.println("  O<value>  - Set crank offset (0-359 degrees)");
          Serial.println("  T[0|1]    - Stop (T0) or start/toggle (T1 or T) output");
          Serial.println("  IK[0|1]   - Disable (IK0), enable (IK1), or toggle (IK) crank invert");
          Serial.println("  IM[0|1]   - Disable (IM0), enable (IM1), or toggle (IM) cam input invert");
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
  Serial.print(" | Crank Enable=");
  Serial.print((output_enable_mask & ENABLE_CRANK_BIT) ? "ON" : "OFF");
  Serial.print(" | Crank Invert=");
  Serial.print((output_invert_mask & INVERT_CRANK_BIT) ? "ON" : "OFF");
  Serial.print(" | Cam input invert=");
  Serial.println((output_invert_mask & INVERT_CAM_BIT) ? "ON" : "OFF");
}