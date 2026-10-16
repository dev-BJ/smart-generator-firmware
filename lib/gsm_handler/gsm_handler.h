#ifndef GSM_HANDLER_H
#define GSM_HANDLER_H

#include "config.h"

void initGsm();
bool initSmsWorker();
void processGsm(String registeredPhone);
void sendSms(String phone, String msg);
bool isAuthorized(String sender, String registeredPhone);
void configureGPRS();
void sendPostRequest(String jsonPayload);
void updateSmsLed();

#endif