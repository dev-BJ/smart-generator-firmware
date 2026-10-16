#include <Arduino.h>
#include "load_transfer.h"
#include "config.h"

static bool loadOnGen = false;
static unsigned long mainsReturnTime = 0;

void initLoadTransfer() {
  pinMode(PIN_RELAY_LOAD, OUTPUT);
  digitalWrite(PIN_RELAY_LOAD, RELAY_OFF);
  loadOnGen = false;
}

void updateLoadTransfer(bool mainsPresent, GenState state) {
  // if (mainsPresent && state != GEN_RUNNING) {
  //   if (mainsReturnTime == 0) {
  //     mainsReturnTime = millis();
  //   } else if (millis() - mainsReturnTime >= MAINS_RETURN_DELAY_MS) {
  //     digitalWrite(PIN_RELAY_LOAD, RELAY_OFF);
  //     updateGeneratorState(GEN_OFF);
  //     loadOnGen = false;
  //     mainsReturnTime = 0;
  //   }
  // } else 
  if (!mainsPresent && state == GEN_RUNNING) {
    digitalWrite(PIN_RELAY_LOAD, RELAY_ON);
    loadOnGen = true;
  } else if (mainsPresent && state == GEN_RUNNING) {
    if (mainsReturnTime == 0) {
      mainsReturnTime = millis();
    } else if (millis() - mainsReturnTime >= MAINS_RETURN_DELAY_MS) {
      digitalWrite(PIN_RELAY_LOAD, RELAY_OFF);
      vTaskDelay(100);
      updateGeneratorState(GEN_OFF);
      loadOnGen = false;
      mainsReturnTime = 0;
    }
  } else if (!mainsPresent) {
    mainsReturnTime = 0;
  }
}

bool isLoadOnGen() { return loadOnGen; }