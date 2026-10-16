#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include "types.h"

void initStorage();
void clearStorage();
void loadPersistentData(String& registeredPhone, unsigned long& genRuntimeDaily, unsigned long& genRuntimeTotal, float& fuelConsumedDaily, float& fuelConsumedTotal, int& lastDay);
void savePersistentData(String registeredPhone, unsigned long genRuntimeDaily, unsigned long genRuntimeTotal, float fuelConsumedDaily, float fuelConsumedTotal, int lastDay, float genVoltage = 0.0f, float mainsVoltage = 0.0f);
void checkDayRollover(int& lastDay, unsigned long& genRuntimeDaily, float& fuelConsumedDaily);
void updateRuntimeStats(unsigned long& genRuntimeDaily, unsigned long& genRuntimeTotal, float& fuelConsumedDaily, float& fuelConsumedTotal, float lastFuelLiters, GenState genState);

#endif