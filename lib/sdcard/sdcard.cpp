#include "sdcard.h"

// External declaration to release RFID bus before SD operations
// extern void rfid_release_bus(void);
static bool mounted = false;
static uint8_t pin_cs;

bool sdcard_init(uint8_t cs_pin) {
    pin_cs = cs_pin;

    if (!SD.begin(pin_cs)) {
        sdcard_deselect();
        return false;
    }
    mounted = true;
    return true;
}

bool sdcard_is_mounted(void) {
    return mounted;
}

File sdcard_open(const char* path, const char* mode) {
    if (!mounted) {
        return File();
    }
    
    File file = SD.open(path, mode);

    return file;
}

bool sdcard_remove(const char* path) {
    bool result = SD.remove(path);
    return result;
}

bool sdcard_exists(const char* path) {
    if (!mounted) {
        return false;
    }

    bool exists = SD.exists(path);

    return exists;
}

bool sdcard_mkdir(const char* path) {
    if (!mounted) {
        return false;
    }
    bool result = SD.mkdir(path);

    return result;
}

bool sdcard_rename(const char* pathFrom, const char* pathTo) {
    if (!mounted) {
        return false;
    }
    bool result = SD.rename(pathFrom, pathTo);

    return result;
}

size_t sdcard_get_free_space(void) {
    // Simplified - returns 0 if not available
    return 0;
}

void sdcard_select(void) {
    digitalWrite(pin_cs, LOW);
}

void sdcard_deselect(void) {
    digitalWrite(pin_cs, HIGH);
}