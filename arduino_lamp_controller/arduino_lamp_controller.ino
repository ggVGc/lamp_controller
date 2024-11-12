// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This example demonstrates Zigbee color dimmer switch.
 *
 * The example demonstrates how to use Zigbee library to control a RGB light
 * bulb. The RGB light bulb is a Zigbee end device, which is controlled by a
 * Zigbee coordinator (Switch). To turn on/off the light, push the button on the
 * switch. To change the color or level of the light, send serial commands to
 * the switch.
 *
 * By setting the switch to allow multiple binding, so it can bind to multiple
 * lights. Also every 30 seconds, all bound lights are printed to the serial
 * console.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition
 * Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include "ZigbeeCore.h"
#include "ep/ZigbeeColorDimmerSwitch.h"

/* Switch configuration */
#define SWITCH_PIN 9 // ESP32-C6/H2 Boot button
#define SWITCH_ENDPOINT_NUMBER 5

/* Zigbee switch */
ZigbeeColorDimmerSwitch zbSwitch =
    ZigbeeColorDimmerSwitch(SWITCH_ENDPOINT_NUMBER);

/********************* Arduino functions **************************/
void setup() {

  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Init button switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  // Optional: set Zigbee device name and model
  zbSwitch.setManufacturerAndModel("ggvgc", "LampController");
  // Optional to allow multiple light to bind to the switch
  zbSwitch.allowMultipleBinding(false);
  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbSwitch);
  // Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(30);
  // When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR mode
  Zigbee.begin(ZIGBEE_COORDINATOR);
  Serial.println("Waiting for light to bind");
  while (!zbSwitch.isBound()) {
    Serial.printf(".");
    delay(500);
  }
  Serial.printf("\nLight bound!\n");
}

struct Color {
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t level)
      : r(r), g(g), b(b), level(level) {
  }

  Color(uint8_t r, uint8_t g, uint8_t b) : Color(r, g, b, 255) {
  }

  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t level;
};

std::vector<Color> colors = {Color(255, 80, 20),   Color(255, 70, 20),
                             Color(255, 50, 15),   Color(255, 40, 0),
                             Color(255, 20, 0),    Color(255, 0, 0),
                             Color(255, 0, 0, 220)};

void loop() {
  static uint32_t click_count = 0;
  if (digitalRead(SWITCH_PIN) == LOW) {
    while (digitalRead(SWITCH_PIN) == LOW) {
      delay(50);
    }

    const Color &color = colors[click_count % colors.size()];
    zbSwitch.setLightColor(color.r, color.g, color.b);
    delay(100);
    zbSwitch.setLightLevel(color.level);
    click_count++;
  }

  if (Serial.available()) {
    uint8_t command;
    const uint8_t bytes_read = Serial.readBytes(&command, 1);

    if (bytes_read == 1) {
      switch (command) {
      case 0: {
        uint8_t colors[3];
        int bytes_read = Serial.readBytes(colors, 3);
        uint8_t red = colors[0];
        uint8_t green = colors[1];
        uint8_t blue = colors[2];
        Serial.printf("Set color: %i, %i, %i\n", red, green, blue);
        zbSwitch.setLightColor(red, green, blue);
      } break;
      case 1: {
        uint8_t level;
        Serial.readBytes(&level, 1);
        Serial.printf("Set level: %i\n", level);
        zbSwitch.setLightLevel(level);
      } break;
      default:
        Serial.printf("Unknown command: %i\n", command);
      }
    }
  }
}
