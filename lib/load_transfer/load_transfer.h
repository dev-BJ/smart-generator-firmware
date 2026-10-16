#ifndef LOAD_TRANSFER_H
#define LOAD_TRANSFER_H

#include "config.h"
#include "types.h"
#include "generator_control.h"

void initLoadTransfer();
void updateLoadTransfer(bool mainsPresent, GenState state);

#endif