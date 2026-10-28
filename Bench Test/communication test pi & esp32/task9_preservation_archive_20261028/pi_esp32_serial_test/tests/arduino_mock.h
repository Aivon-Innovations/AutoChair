#pragma once
#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <strings.h>

inline uint32_t mock_millis_val = 1000;
inline uint32_t millis() { return mock_millis_val; }
inline void delay(uint32_t ms) { mock_millis_val += ms; }

// Minimal Arduino String emulation using std::string
class ArduinoString : public std::string {
public:
    ArduinoString() : std::string() {}
    ArduinoString(const char* s) : std::string(s ? s : "") {}
    ArduinoString(const std::string& s) : std::string(s) {}
};
using String = ArduinoString;

struct MockSerialClass {
    std::string buffer;
    size_t write(uint8_t c) { buffer += (char)c; return 1; }
    size_t write(const uint8_t* s, size_t n) { if (s && n) buffer.append((const char*)s, n); return n; }
    void print(char c) { buffer += c; }
    void print(const char* s) { if (s) buffer += s; }
    void print(const std::string& s) { buffer += s; }
    void println(const char* s = "") { if (s) buffer += s; buffer += "\n"; }
    void clear() { buffer.clear(); }
};
inline MockSerialClass Serial;
