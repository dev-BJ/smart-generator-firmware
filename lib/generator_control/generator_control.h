#ifndef GENERATOR_CONTROL_H
#define GENERATOR_CONTROL_H

#include "types.h"

void initGeneratorControl();
void updateGeneratorControl(GenState* state, bool mainsPresent, bool genVoltagePresent, String registeredPhone, float mainsVoltage, float genVoltage);
GenState getGenState();
void startGenerator();
void stopGenerator();
unsigned long getGenRuntime();
float getLastFuelLiters();
void setLastFuelLiters(float v);
void updateGeneratorState(GenState newState);

#endif