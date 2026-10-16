#ifndef FUEL_MONITOR_H
#define FUEL_MONITOR_H

#include "config.h"
#include "types.h"

float measureDistance();
float calcFuelPercent(float distCm);
float filterFuel(float newVal);
void updateFuelPercent();
void updateFuelAlerts(String registeredPhone);
float getFuelPercent();
float getFuelLiters();
void resetFuelAlerts();

#endif