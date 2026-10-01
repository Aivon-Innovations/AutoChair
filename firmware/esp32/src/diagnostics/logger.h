/**
 * @file logger.h
 * @brief AutoChair ESP32 — Logging / diagnostics abstraction
 *
 * Project:  Aivon Innovations — AutoChair
 * Platform: ESP32-WROOM-32D
 * Status:   IN DEVELOPMENT
 *
 * Provides a structured logging interface used by all firmware modules.
 * On the target hardware the output goes to the serial port.
 * In the native test environment output goes to stdout.
 *
 * Log levels (ESP32_CONTROLLER_PRD.md §21):
 *   DEBUG / INFO / WARNING / ERROR / FAULT
 *
 * Safety-critical faults use FAULT level and are clearly distinguishable
 * from normal informational messages.
 */

#pragma once

#include "autochair_types.h"
#include <cstdint>

namespace autochair {

class Logger {
public:
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /** Call once during firmware startup before any logging. */
    static void begin(uint32_t baudRate = 115200);

    /** Set the minimum log level (messages below this are suppressed). */
    static void setLevel(LogLevel level);

    // -------------------------------------------------------------------------
    // Logging methods
    // -------------------------------------------------------------------------

    static void debug(const char* module, const char* message);
    static void info(const char* module, const char* message);
    static void warning(const char* module, const char* message);
    static void error(const char* module, const char* message);

    /**
     * @brief Log a safety-critical fault.
     * FAULT messages are always emitted regardless of the configured level.
     */
    static void fault(const char* module, const char* message);

    // -------------------------------------------------------------------------
    // Formatted logging (convenience wrappers)
    // -------------------------------------------------------------------------

    static void logf(LogLevel level, const char* module,
                     const char* format, ...);

private:
    static LogLevel _level;

    static void emit(LogLevel level, const char* module, const char* message);
    static const char* levelName(LogLevel level);
};

}  // namespace autochair
