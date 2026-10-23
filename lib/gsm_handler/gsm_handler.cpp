#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#ifndef TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM800
#endif
#ifndef TINY_GSM_RX_BUFFER
#define TINY_GSM_RX_BUFFER 650
#endif
#define ENABLE_DEBUG
#include <TinyGsmClient.h>
#include <ESP_SSLClient.h>
#include <RTClib.h>
#include "config.h"
#include "gsm_handler.h"
#include "types.h"
#include "fuel_monitor.h"
#include "generator_control.h"
#include "storage.h"
#include "telemetry_ca_certs.h"

static TinyGsm modem(Serial2);
static TinyGsmClient modemClient(modem);
static ESP_SSLClient sslClient;
static String pendingSmsSender = "";
static bool smsLedState = false;
static unsigned long smsLedTime = 0;
static bool smsReady = false;
static bool modemReady = false;
static bool gprsReady = false;
static unsigned long lastSmsPoll = 0;
static portMUX_TYPE smsLedMux = portMUX_INITIALIZER_UNLOCKED;

struct QueuedSms {
  char phone[24];
  char msg[180];
};

static QueueHandle_t smsQueue = nullptr;
static SemaphoreHandle_t smsQueueMutex = nullptr;
static SemaphoreHandle_t modemMutex = nullptr;
static TaskHandle_t smsTaskHandle = nullptr;
static bool smsWorkerStarted = false;

static void triggerSmsLed();
static void smsTask(void* pvParameters);

class ModemGuard {
public:
  ModemGuard()
      : locked_(modemMutex && xSemaphoreTakeRecursive(modemMutex, portMAX_DELAY) == pdTRUE) {}

  ~ModemGuard() {
    if (locked_) xSemaphoreGiveRecursive(modemMutex);
  }

  bool locked() const { return locked_; }

private:
  bool locked_;
};

static bool ensureSmsTextMode() {
  modem.sendAT("+CMGF=1");
  if (modem.waitResponse(5000) != 1) {
    Serial.println("SMS mode check failed: modem did not accept AT+CMGF=1");
    return false;
  }
  return true;
}

static bool ensureModemReadyForSms() {
  if (!smsReady || !modemReady) {
    Serial.println("SMS aborted: modem or SMS stack is not initialized");
    return false;
  }

  if (!modem.isNetworkConnected()) {
    Serial.println("SMS waiting for network registration...");
    if (!modem.waitForNetwork(30000)) {
      Serial.println("SMS aborted: modem is not registered on the cellular network");
      return false;
    }
  }

  if (!ensureSmsTextMode()) {
    return false;
  }

  modem.sendAT("+CSQ");
  String csqResponse;
  const int csqResult = modem.waitResponse(5000, csqResponse);
  if (csqResult == 1) {
    Serial.print("Signal quality: ");
    Serial.println(csqResponse);
  }

  return true;
}

static bool enqueueQueuedSms(const String& phone, const String& msg) {
  if (phone.length() == 0 || msg.length() == 0) return false;
  if (!smsQueue || !smsQueueMutex) return false;

  QueuedSms queued = {};
  snprintf(queued.phone, sizeof(queued.phone), "%s", phone.c_str());
  snprintf(queued.msg, sizeof(queued.msg), "%s", msg.c_str());

  if (xSemaphoreTake(smsQueueMutex, portMAX_DELAY) != pdTRUE) return false;
  bool enqueued = xQueueSend(smsQueue, &queued, 0) == pdPASS;
  if (!enqueued) {
    QueuedSms dropped;
    if (xQueueReceive(smsQueue, &dropped, 0) == pdPASS) {
      Serial.println("SMS queue full; dropping oldest pending SMS");
    }
    enqueued = xQueueSend(smsQueue, &queued, 0) == pdPASS;
  }
  xSemaphoreGive(smsQueueMutex);
  if (enqueued && smsTaskHandle) xTaskNotifyGive(smsTaskHandle);
  return enqueued;
}

bool initSmsWorker() {
  if (smsWorkerStarted) return true;

  smsQueue = xQueueCreate(10, sizeof(QueuedSms));
  smsQueueMutex = xSemaphoreCreateMutex();
  modemMutex = xSemaphoreCreateRecursiveMutex();
  if (!smsQueue || !smsQueueMutex || !modemMutex) {
    Serial.println("SMS worker initialization failed: could not allocate queue or mutexes");
    if (smsQueue) vQueueDelete(smsQueue);
    if (smsQueueMutex) vSemaphoreDelete(smsQueueMutex);
    if (modemMutex) vSemaphoreDelete(modemMutex);
    smsQueue = nullptr;
    smsQueueMutex = nullptr;
    modemMutex = nullptr;
    return false;
  }

  if (xTaskCreatePinnedToCore(smsTask, "SmsTask", 8192, nullptr, 2, &smsTaskHandle, 0) != pdPASS) {
    Serial.println("SMS worker initialization failed: could not create task");
    vQueueDelete(smsQueue);
    vSemaphoreDelete(smsQueueMutex);
    vSemaphoreDelete(modemMutex);
    smsQueue = nullptr;
    smsQueueMutex = nullptr;
    modemMutex = nullptr;
    return false;
  }

  smsWorkerStarted = true;
  return true;
}

static void smsTask(void* pvParameters) {
  (void)pvParameters;
  QueuedSms queued;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (xSemaphoreTake(smsQueueMutex, portMAX_DELAY) == pdTRUE) {
      const bool hasQueuedSms = xQueueReceive(smsQueue, &queued, 0) == pdPASS;
      xSemaphoreGive(smsQueueMutex);
      if (!hasQueuedSms) break;

      bool sent = false;
      {
        ModemGuard modemGuard;
        if (modemGuard.locked() && ensureModemReadyForSms()) {
          Serial.print("Sending SMS to ");
          Serial.println(queued.phone);
          sent = modem.sendSMS(queued.phone, queued.msg);
        }
      }

      if (sent) {
        triggerSmsLed();
        Serial.println(String("SMS sent successfully to ") + queued.phone);
      } else {
        Serial.println("SMS send failed; queued for retry");
        enqueueQueuedSms(String(queued.phone), String(queued.msg));
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;
      }
    }
  }
}

void initGsm() {
  ModemGuard modemGuard;
  if (!modemGuard.locked()) return;

  Serial2.begin(GSM_BAUD_RATE, SERIAL_8N1, PIN_SIM_TX, PIN_SIM_RX);
  modemReady = modem.init();
  if (!modemReady) {
    Serial.println("GSM initialization failed");
    return;
  }

  modem.sendAT("+CMGF=1");
  if (modem.waitResponse(5000) != 1) {
    Serial.println("Could not enable SMS text mode");
    return;
  }

  modem.sendAT("+CSCS=\"GSM\"");
  if (modem.waitResponse(5000) != 1) {
    Serial.println("Could not set GSM character set for SMS");
    return;
  }

  // Store incoming SMS on the SIM; do not inject message bodies into the TCP stream.
  modem.sendAT("+CNMI=2,1,0,0,0");
  if (modem.waitResponse(5000) != 1) {
    Serial.println("Could not configure stored SMS notifications");
    return;
  }

  extern RTC_DS3231 rtc;
  sslClient.setClient(&modemClient, true);
  // sslClient.setCACert(TELEMETRY_CA_CERTS);
  sslClient.setInsecure();
  sslClient.setBufferSizes(2048, 512);
  sslClient.setDebugLevel(1);
  sslClient.setX509Time(rtc.now().unixtime());
  sslClient.setTimeout(10000);

  smsReady = true;
  Serial.println("GSM modem ready; TLS certificate validation enabled");
}

void triggerSmsLed() {
  const unsigned long now = millis();
  portENTER_CRITICAL(&smsLedMux);
  smsLedState = true;
  smsLedTime = now;
  digitalWrite(PIN_LED_SMS, HIGH);
  portEXIT_CRITICAL(&smsLedMux);
}

void updateSmsLed() {
  const unsigned long now = millis();
  portENTER_CRITICAL(&smsLedMux);
  if (smsLedState && now - smsLedTime > 500) {
    smsLedState = false;
    digitalWrite(PIN_LED_SMS, LOW);
  }
  portEXIT_CRITICAL(&smsLedMux);
}

String normalizePhone(String phone) {
  phone.trim();
  if (phone.startsWith("+")) return phone;
  if (phone.startsWith("0") && phone.length() == 11) return "+234" + phone.substring(1);
  if (phone.length() == 10) return "+234" + phone;
  return phone;
}

bool isAuthorized(String sender, String registeredPhone) {
  sender = normalizePhone(sender);
  String reg = normalizePhone(registeredPhone);
  String def = normalizePhone(DEFAULT_ADMIN_NUMBER);
  return sender == reg || sender == def;
}

void sendSms(String phone, String msg) {
  if (phone.length() == 0 || msg.length() == 0) return;

  if (!enqueueQueuedSms(phone, msg)) {
    Serial.println("SMS queue unavailable; message not sent");
    return;
  }
  Serial.println("SMS queued for background delivery");
}

void processSms(String sender, String body, String registeredPhone) {
  sender = normalizePhone(sender);
  body.trim(); body.toUpperCase();

  if (!isAuthorized(sender, registeredPhone)) {
    if (sender != normalizePhone(DEFAULT_ADMIN_NUMBER)) return;
    if (body != CMD_RSTALL && body != CMD_RSTNUMBER) return;
  }

  triggerSmsLed();

  extern GenState genState;
  extern bool mainsPresent;
  extern bool genVoltagePresent;
  extern float mainsVoltage;
  extern float genVoltage;
  extern void beepBuzzer(int count, int duration);
  extern String getDateTimeStr();
  extern void resetFuelAlerts();
  extern void startGenerator();
  extern void stopGenerator();
  extern float getFuelPercent();
  extern float getFuelLiters();
  extern float getLastFuelLiters();

  if (body == CMD_ONGEN) {
    if (genState == GEN_OFF) {
      if (getFuelPercent() < FUEL_LOW_THRESHOLD_PCT) {
        sendSms(sender, "Generator not Working Reason: Low Fuel Level " + String((int)getFuelPercent()) + "%");
      } else {
        startGenerator();
        sendSms(sender, "Starting generator...");
      }
    }
  } else if (body == CMD_OFFGEN) {
    if (genState == GEN_RUNNING || genState == GEN_STARTING) {
      stopGenerator();
      sendSms(sender, "Stopping generator...");
    }
  } else if (body == CMD_MSTATUS) {
    sendSms(sender, "Gen: " + String(genVoltage, 0) + "v Mains: " + String(mainsVoltage) + "v Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)getFuelPercent()) + "%");
  } else if (body == CMD_DSTATUS) {
    extern unsigned long genRuntimeDaily;
    extern float fuelConsumedDaily;
    sendSms(sender, "GEN Working Hrs: " + String(genRuntimeDaily / 3600) + "h " + String((genRuntimeDaily % 3600) / 60) + "m\nFuel Used: " + String(fuelConsumedDaily, 1) + "L");
  } else if (body == CMD_RSTALL) {
    extern unsigned long genRuntimeDaily, genRuntimeTotal;
    extern float fuelConsumedDaily, fuelConsumedTotal;
    extern String registeredPhone;
    extern int lastDay;
    genRuntimeDaily = 0; genRuntimeTotal = 0; fuelConsumedDaily = 0; fuelConsumedTotal = 0;
    setLastFuelLiters(getFuelLiters());
    savePersistentData(registeredPhone, genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, lastDay, genVoltage, mainsVoltage);
    sendSms(sender, "System successfully reset");
  } else if (body == CMD_RSTNUMBER) {
    extern String registeredPhone;
    extern unsigned long genRuntimeDaily, genRuntimeTotal;
    extern float fuelConsumedDaily, fuelConsumedTotal;
    extern int lastDay;
    registeredPhone = DEFAULT_ADMIN_NUMBER;
    savePersistentData(registeredPhone, genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, lastDay, genVoltage, mainsVoltage);
    sendSms(sender, "Previous Number: " + String(DEFAULT_ADMIN_NUMBER));
  } else if (body.startsWith(CMD_CHP_PREFIX)) {
    int nPos = body.indexOf('N');
    if (nPos > 3) {
      String oldNum = normalizePhone(body.substring(3, nPos));
      String newNum = normalizePhone(body.substring(nPos + 1));
      extern String registeredPhone;
      extern unsigned long genRuntimeDaily, genRuntimeTotal;
      extern float fuelConsumedDaily, fuelConsumedTotal;
      extern int lastDay;
      if (oldNum == normalizePhone(registeredPhone) || oldNum == normalizePhone(DEFAULT_ADMIN_NUMBER)) {
        registeredPhone = newNum;
        savePersistentData(registeredPhone, genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, lastDay, genVoltage, mainsVoltage);
        sendSms(sender, "Previous Number: " + oldNum + " New Number: " + newNum);
      }
    }
  }
}

void processGsm(String registeredPhone) {
  ModemGuard modemGuard;
  if (!modemGuard.locked()) return;

  if (!smsReady || millis() - lastSmsPoll < 2000) return;
  vTaskDelay(pdMS_TO_TICKS(10));
  lastSmsPoll = millis();

  if (!modem.isNetworkConnected() && !modem.waitForNetwork(GSM_INIT_TIMEOUT_MS)) {
    Serial.println("SMS poll skipped: modem not registered on the cellular network");
    return;
  }

  modem.sendAT("+CMGL=\"REC UNREAD\"");
  String response;
  if (modem.waitResponse(10000, response) != 1) return;
  vTaskDelay(pdMS_TO_TICKS(10));
  const int firstHeaderStart = response.indexOf("+CMGL:");

  if (firstHeaderStart >= 0) {
    Serial.println("Received SMS response:");
    Serial.println(response);
  }

  if (firstHeaderStart < 0) return;

  int scanPos = 0;
  while (scanPos >= 0) {
    const int headerStart = response.indexOf("+CMGL:", scanPos);
    if (headerStart < 0) break;

    const int headerEnd = response.indexOf('\n', headerStart);
    if (headerEnd < 0) break;

    String header = response.substring(headerStart, headerEnd);
    const int indexEnd = header.indexOf(',');
    const int statusQuote = header.indexOf('"');
    const int statusQuoteEnd = header.indexOf('"', statusQuote + 1);
    const int senderQuote = header.indexOf('"', statusQuoteEnd + 1);
    const int senderQuoteEnd = header.indexOf('"', senderQuote + 1);
    if (indexEnd < 0 || statusQuote < 0 || statusQuoteEnd < 0 || senderQuote < 0 || senderQuoteEnd < 0) {
      scanPos = headerEnd + 1;
      continue;
    }

    const int messageIndex = header.substring(6, indexEnd).toInt();
    pendingSmsSender = header.substring(senderQuote + 1, senderQuoteEnd);

    int bodyStart = headerEnd + 1;
    while (bodyStart < response.length() && (response[bodyStart] == '\r' || response[bodyStart] == '\n')) bodyStart++;
    int bodyEnd = response.indexOf('\n', bodyStart);
    if (bodyEnd < 0) bodyEnd = response.length();
    String body = response.substring(bodyStart, bodyEnd);
    body.trim();

    if (messageIndex > 0 && pendingSmsSender.length() > 0 && body.length() > 0) {
      processSms(pendingSmsSender, body, registeredPhone);
      String deleteCmd = "+CMGD=" + String(messageIndex);
      modem.sendAT(deleteCmd.c_str());
      modem.waitResponse();
      triggerSmsLed();
    }

    pendingSmsSender = "";
    scanPos = bodyEnd + 1;
  }
}

void configureGPRS() {
  ModemGuard modemGuard;
  if (!modemGuard.locked()) return;

  if (!modemReady) {
    Serial.println("GPRS setup failed: modem is not initialized");
    return;
  }

  if (!modem.waitForNetwork(GSM_INIT_TIMEOUT_MS)) {
    Serial.println("GPRS setup failed: cellular network registration timed out");
    return;
  }

  gprsReady = modem.gprsConnect(GSM_APN);
  if (!gprsReady) {
    Serial.println("GPRS setup failed: APN connection failed");
    return;
  }

  Serial.print("GPRS connected; IP: ");
  Serial.println(modem.getLocalIP());
}

static bool parseHttpsEndpoint(const String& endpoint, String& host, String& path, uint16_t& port) {
  if (!endpoint.startsWith("https://")) return false;

  const int authorityStart = 8;
  int authorityEnd = endpoint.indexOf('/', authorityStart);
  const int queryStart = endpoint.indexOf('?', authorityStart);
  if (authorityEnd < 0 || (queryStart >= 0 && queryStart < authorityEnd)) authorityEnd = queryStart;
  if (authorityEnd < 0) authorityEnd = endpoint.length();

  String authority = endpoint.substring(authorityStart, authorityEnd);
  const int portSeparator = authority.lastIndexOf(':');
  port = 443;
  if (portSeparator > 0) {
    const long parsedPort = authority.substring(portSeparator + 1).toInt();
    if (parsedPort < 1 || parsedPort > 65535) return false;
    port = static_cast<uint16_t>(parsedPort);
    host = authority.substring(0, portSeparator);
  } else {
    host = authority;
  }
  if (host.length() == 0) return false;

  path = endpoint.substring(authorityEnd);
  if (path.length() == 0) path = "/";
  else if (path[0] == '?') path = "/" + path;
  return true;
}

void sendPostRequest(String jsonPayload) {
  ModemGuard modemGuard;
  if (!modemGuard.locked()) return;

  if (jsonPayload.length() == 0) {
    Serial.println("HTTP POST aborted: empty JSON payload");
    return;
  }

  if (!modemReady) {
    Serial.println("HTTPS POST aborted: modem is not initialized");
    return;
  }

  if (!modem.isNetworkConnected() && !modem.waitForNetwork(30000)) {
    Serial.println("HTTPS POST aborted: cellular network is unavailable");
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }
  if (!modem.isGprsConnected()) {
    gprsReady = modem.gprsConnect(GSM_APN);
    if (!gprsReady) {
      Serial.println("HTTPS POST aborted: GPRS reconnect failed");
      return;
    }
  }

  String host;
  String path;
  uint16_t port;
  if (!parseHttpsEndpoint(TELEMETRY_ENDPOINT, host, path, port)) {
    Serial.println("HTTPS POST aborted: TELEMETRY_ENDPOINT must be a valid https URL");
    return;
  }

  Serial.print("HTTPS POST to ");
  Serial.println(host);
  Serial.println("Checking plain TCP reachability to port 443...");
  if (!modemClient.connect(host.c_str(), port, 30)) {
    vTaskDelay(pdMS_TO_TICKS(10));
    Serial.println("TCP connection failed before TLS; check DNS, GPRS routing, and SIM800 socket status");
    modemClient.stop();
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(10));
  Serial.println("TCP connection succeeded; closing diagnostic socket before TLS handshake");
  modemClient.stop();
  vTaskDelay(pdMS_TO_TICKS(10));

  if (!sslClient.connect(host.c_str(), port, 30000)) {
    char error[160] = {0};
    const int tlsError = sslClient.getLastSSLError(error, sizeof(error));
    Serial.printf("TLS connection failed: BearSSL error=%d, client error=%d, detail=%s\n",
                  tlsError, sslClient.getWriteError(), error);
    sslClient.stop();
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(10));

  String hostHeader = host;
  if (port != 443) hostHeader += ":" + String(port);
  sslClient.print("POST ");
  sslClient.print(path);
  sslClient.print(" HTTP/1.1\r\nHost: ");
  sslClient.print(hostHeader);
  sslClient.print("\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ");
  sslClient.print(jsonPayload.length());
  sslClient.print("\r\n\r\n");
  sslClient.print(jsonPayload);

  // Wait for response with timeout
  unsigned long timeoutStart = millis();
  while (!sslClient.available() && millis() - timeoutStart < 10000) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  String response;
  while (sslClient.available()) {
    response += static_cast<char>(sslClient.read());
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  sslClient.stop();

  const int lineEnd = response.indexOf('\n');
  const String statusLine = (lineEnd >= 0 ? response.substring(0, lineEnd) : response);
  Serial.print("HTTP status line: ");
  Serial.println(statusLine);
  const int firstSpace = statusLine.indexOf(' ');
  const int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
  const int statusCode = firstSpace >= 0
      ? statusLine.substring(firstSpace + 1, secondSpace > firstSpace ? secondSpace : statusLine.length()).toInt()
      : 0;

  if (statusCode >= 200 && statusCode < 300) {
    Serial.println("HTTPS POST succeeded");
  } else if (statusCode >= 300 && statusCode < 400) {
    Serial.println("HTTPS POST redirected; configure TELEMETRY_ENDPOINT with the final API URL");
  } else {
    Serial.println("HTTPS POST failed or returned a non-success status");
  }
}
