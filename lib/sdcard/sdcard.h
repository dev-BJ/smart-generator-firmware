#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sdcard_init(uint8_t cs_pin);
bool sdcard_is_mounted(void);
File sdcard_open(const char* path, const char* mode);
bool sdcard_exists(const char* path);
bool sdcard_mkdir(const char* path);
bool sdcard_rename(const char* pathFrom, const char* pathTo);
size_t sdcard_get_free_space(void);
void sdcard_select(void);
void sdcard_deselect(void);
bool sdcard_remove(const char* path);

#ifdef __cplusplus
}
#endif

#endif // SDCARD_H