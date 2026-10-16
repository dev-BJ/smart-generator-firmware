#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "types.h"

void initDisplay();
void updateDisplay(bool mainsPresent, GenState genState, float fuelPercent, float fuelLiters, float batteryVoltage, float mainsVoltage, float genVoltage);

#endif