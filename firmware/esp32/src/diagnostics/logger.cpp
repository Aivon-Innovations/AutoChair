/**
 * @file logger.cpp
 * @brief AutoChair ESP32 — Logging implementation
 *
 * On Arduino/ESP32: uses Serial.println().
 * On native (test): uses printf() to stdout.
 */

#include "logger.h"

#include <cstdio>
#include <cstdarg>

#ifdef ENV_NATIVE
#  include <chrono>
static uint32_t millis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}
#else
#  include <Arduino.h>
#endif

namespace autochair {

// Default minimum level: DEBUG (show all messages during development).
LogLevel Logger::_level = LogLevel::DEBUG;

void Logger::begin(uint32_t baudRate) {
#ifndef ENV_NATIVE
    Serial.begin(baudRate);
    while (!Serial) { /* wait for USB serial */ }
#else
    (void)baudRate;
#endif
    emit(LogLevel::INFO, "Logger", "AutoChair ESP32 logging started");
    emit(LogLevel::INFO, "Logger", "Firmware v0.1.0");
}

void Logger::setLevel(LogLevel level) {
    _level = level;
}

void Logger::debug(const char* module, const char* message) {
    emit(LogLevel::DEBUG, module, message);
}

void Logger::info(const char* module, const char* message) {
    emit(LogLevel::INFO, module, message);
}

void Logger::warning(const char* module, const char* message) {
    emit(LogLevel::WARNING, module, message);
}

void Logger::error(const char* module, const char* message) {
    emit(LogLevel::ERROR, module, message);
}

void Logger::fault(const char* module, const char* message) {
    // FAULT always emitted regardless of the configured level.
    // Uses direct output to ensure visibility even if level filtering is set high.
    char buf[256];
    snprintf(buf, sizeof(buf), "[FAULT][%lu][%s] %s",
             (unsigned long)millis(), module, message);
#ifdef ENV_NATIVE
    printf("%s\n", buf);
#else
    Serial.println(buf);
#endif
}

void Logger::logf(LogLevel level, const char* module, const char* format, ...) {
    char msg[192];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    emit(level, module, msg);
}

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

void Logger::emit(LogLevel level, const char* module, const char* message) {
    if (level < _level) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "[%s][%lu][%s] %s",
             levelName(level), (unsigned long)millis(), module, message);

#ifdef ENV_NATIVE
    printf("%s\n", buf);
#else
    Serial.println(buf);
#endif
}

const char* Logger::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FAULT:   return "FAULT";
        default:                return "?";
    }
}

}  // namespace autochair
