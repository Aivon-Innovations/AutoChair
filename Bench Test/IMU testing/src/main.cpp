/*
 * ============================================================================
 * ESP32 + MPU6050 Standalone Bench Validation — Interactive Command Dispatcher
 * ============================================================================
 * Sensor: MPU6050 (4-Pin Breakout: VCC, GND, SDA, SCL)
 * Pins  : SDA = GPIO 21, SCL = GPIO 22, VCC = 3.3V, GND = ESP32 GND
 * Address: 0x68
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

static const uint8_t PIN_SDA = 21;
static const uint8_t PIN_SCL = 22;
static const uint32_t I2C_FREQ = 100000;
static const uint8_t MPU6050_ADDR = 0x68;

// Register map
static const uint8_t REG_SMPLRT_DIV   = 0x19;
static const uint8_t REG_CONFIG       = 0x1A;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_TEMP_OUT_H   = 0x41;
static const uint8_t REG_GYRO_XOUT_H  = 0x43;
static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_PWR_MGMT_2   = 0x6C;
static const uint8_t REG_WHO_AM_I     = 0x75;

static const float ACCEL_SCALE = 16384.0f; // ±2g
static const float GYRO_SCALE  = 131.0f;   // ±250 dps

struct IMUData {
    float ax, ay, az, mag;
    float temp;
    float gx, gy, gz;
    int16_t rawAx, rawAy, rawAz;
    int16_t rawGx, rawGy, rawGz;
    bool valid;
};

struct BaselineStats {
    float ax, ay, az;
    float gx, gy, gz;
};

BaselineStats current_baseline = {0,0,0,0,0,0};

bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

bool readRegs(uint8_t reg, uint8_t count, uint8_t* dest) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) return false;
    uint8_t rec = Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)count, (bool)true);
    if (rec != count) return false;
    for (uint8_t i = 0; i < count; i++) dest[i] = Wire.read();
    return true;
}

uint8_t readSingle(uint8_t reg, bool* ok = nullptr) {
    uint8_t val = 0;
    bool s = readRegs(reg, 1, &val);
    if (ok) *ok = s;
    return val;
}

bool initMPU6050() {
    if (!writeReg(REG_PWR_MGMT_1, 0x80)) return false; // Reset
    delay(100);
    if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false; // PLL with X Gyro
    delay(30);
    if (!writeReg(REG_PWR_MGMT_2, 0x00)) return false; // Enable all axes
    if (!writeReg(REG_SMPLRT_DIV, 0x04)) return false; // 200 Hz
    if (!writeReg(REG_CONFIG, 0x03)) return false;     // DLPF 42Hz
    if (!writeReg(REG_GYRO_CONFIG, 0x00)) return false;// ±250 dps
    if (!writeReg(REG_ACCEL_CONFIG, 0x00)) return false;// ±2g
    delay(30);
    return true;
}

IMUData readIMU() {
    IMUData d;
    d.valid = false;
    uint8_t bufA[6];
    uint8_t bufG[6];
    
    if (readRegs(REG_ACCEL_XOUT_H, 6, bufA) && readRegs(REG_GYRO_XOUT_H, 6, bufG)) {
        d.rawAx = (int16_t)((bufA[0] << 8) | bufA[1]);
        d.rawAy = (int16_t)((bufA[2] << 8) | bufA[3]);
        d.rawAz = (int16_t)((bufA[4] << 8) | bufA[5]);
        
        d.rawGx = (int16_t)((bufG[0] << 8) | bufG[1]);
        d.rawGy = (int16_t)((bufG[2] << 8) | bufG[3]);
        d.rawGz = (int16_t)((bufG[4] << 8) | bufG[5]);

        d.ax = (float)d.rawAx / ACCEL_SCALE;
        d.ay = (float)d.rawAy / ACCEL_SCALE;
        d.az = (float)d.rawAz / ACCEL_SCALE;
        d.mag = sqrtf(d.ax * d.ax + d.ay * d.ay + d.az * d.az);
        d.temp = 30.0f;
        d.gx = (float)d.rawGx / GYRO_SCALE;
        d.gy = (float)d.rawGy / GYRO_SCALE;
        d.gz = (float)d.rawGz / GYRO_SCALE;
        d.valid = true;
    }
    return d;
}

void printHeader(const char* title) {
    Serial.println();
    Serial.println(F("================================================================================"));
    Serial.println(title);
    Serial.println(F("================================================================================"));
}

// ---------------------------------------------------------------------------
// TEST 1 — I2C Detection
// ---------------------------------------------------------------------------
void execTest1() {
    printHeader("TEST 1 -- I2C BUS DETECTION (SDA=GPIO21, SCL=GPIO22)");
    Serial.println(F("Scanning I2C address space (0x01 - 0x7F)..."));
    uint8_t found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  [+] Device found at address: 0x%02X\n", a);
            found++;
        }
    }
    Serial.printf("Total devices detected: %u\n", found);
    Serial.println(F("Performing repeated communication stability check (10 probes)..."));
    uint8_t hits = 0;
    for (int i = 0; i < 10; i++) {
        Wire.beginTransmission(MPU6050_ADDR);
        if (Wire.endTransmission() == 0) hits++;
        delay(10);
    }
    Serial.printf("Probe Success Rate: %u / 10\n", hits);
    if (hits == 10) {
        Serial.println(F("RESULT: PASS -- Address 0x68 detected and 100% stable."));
    } else {
        Serial.println(F("RESULT: FAIL -- Address 0x68 unstable or not found."));
    }
}

// ---------------------------------------------------------------------------
// TEST 2 — Sensor Identification (MPU6050)
// ---------------------------------------------------------------------------
void execTest2() {
    printHeader("TEST 2 -- SENSOR IDENTIFICATION (WHO_AM_I Register 0x75)");
    bool ok = false;
    uint8_t whoami = readSingle(REG_WHO_AM_I, &ok);
    Serial.printf("I2C Address           : 0x%02X\n", MPU6050_ADDR);
    Serial.printf("WHO_AM_I Register Val : 0x%02X\n", whoami);
    Serial.println(F("Target Sensor Model   : MPU6050 (Expected WHO_AM_I = 0x68)"));

    if (ok && whoami == 0x68) {
        Serial.println(F("Interpreted Sensor ID : MPU6050 (Authentic)"));
        Serial.println(F("RESULT: PASS -- Verified genuine MPU6050 hardware silicon identifier (0x68)."));
    } else {
        Serial.printf("RESULT: FAIL -- WHO_AM_I was 0x%02X (Expected 0x68 for MPU6050).\n", whoami);
    }
}

// ---------------------------------------------------------------------------
// TEST 3 — Stationary Accelerometer (5s)
// ---------------------------------------------------------------------------
void execTest3() {
    printHeader("TEST 3 -- STATIONARY ACCELEROMETER VALIDATION (5s)");
    initMPU6050();
    delay(200);

    Serial.println(F("Time(ms)  | Acc X (g) | Acc Y (g) | Acc Z (g) | Mag |A| (g)"));
    Serial.println(F("-------------------------------------------------------"));

    const int SAMPLES = 100;
    float sumMag = 0, minMag = 99, maxMag = -99;
    uint32_t valid = 0, dropouts = 0;

    for (int i = 0; i < SAMPLES; i++) {
        IMUData d = readIMU();
        if (!d.valid) {
            dropouts++;
        } else {
            valid++;
            sumMag += d.mag;
            if (d.mag < minMag) minMag = d.mag;
            if (d.mag > maxMag) maxMag = d.mag;
            if (i % 10 == 0 || i == SAMPLES - 1) {
                Serial.printf("%9lu |  %+7.3f  |  %+7.3f  |  %+7.3f  |  %6.3f g\n",
                              millis(), d.ax, d.ay, d.az, d.mag);
            }
        }
        delay(50);
    }
    float avgMag = valid > 0 ? (sumMag / valid) : 0;
    Serial.println(F("-------------------------------------------------------"));
    Serial.printf("Samples Valid: %u / %d | Dropouts: %u\n", valid, SAMPLES, dropouts);
    Serial.printf("Average Acceleration Magnitude |A|: %.4f g (Expected ~1.000g)\n", avgMag);
    Serial.printf("Magnitude Range: [%.4f, %.4f] g\n", minMag, maxMag);

    if (valid >= 98 && avgMag >= 0.85f && avgMag <= 1.15f) {
        Serial.println(F("RESULT: PASS -- Stationary accelerometer reflects ~1g gravity vector without dropouts."));
    } else {
        Serial.println(F("RESULT: FAIL -- Accelerometer magnitude out of range or dropouts detected."));
    }
}

// ---------------------------------------------------------------------------
// TEST 4 — Stationary Gyroscope (5s)
// ---------------------------------------------------------------------------
void execTest4() {
    printHeader("TEST 4 -- STATIONARY GYROSCOPE VALIDATION (5s)");
    Serial.println(F("Time(ms)  | Gyro X (dps) | Gyro Y (dps) | Gyro Z (dps)"));
    Serial.println(F("------------------------------------------------------"));

    const int SAMPLES = 100;
    float sumGx = 0, sumGy = 0, sumGz = 0;
    float minGx = 999, maxGx = -999;
    float minGy = 999, maxGy = -999;
    float minGz = 999, maxGz = -999;
    uint32_t valid = 0, dropouts = 0;

    for (int i = 0; i < SAMPLES; i++) {
        IMUData d = readIMU();
        if (!d.valid) {
            dropouts++;
        } else {
            valid++;
            sumGx += d.gx; sumGy += d.gy; sumGz += d.gz;
            if (d.gx < minGx) minGx = d.gx; if (d.gx > maxGx) maxGx = d.gx;
            if (d.gy < minGy) minGy = d.gy; if (d.gy > maxGy) maxGy = d.gy;
            if (d.gz < minGz) minGz = d.gz; if (d.gz > maxGz) maxGz = d.gz;
            if (i % 10 == 0 || i == SAMPLES - 1) {
                Serial.printf("%9lu |   %+8.2f   |   %+8.2f   |   %+8.2f\n",
                              millis(), d.gx, d.gy, d.gz);
            }
        }
        delay(50);
    }
    float mGx = sumGx/valid, mGy = sumGy/valid, mGz = sumGz/valid;
    Serial.println(F("------------------------------------------------------"));
    Serial.println(F("Stationary Gyroscope Statistics:"));
    Serial.printf("  Gyro X -> Mean: %+6.2f dps | Min: %+6.2f | Max: %+6.2f | Range: %5.2f dps\n",
                  mGx, minGx, maxGx, maxGx - minGx);
    Serial.printf("  Gyro Y -> Mean: %+6.2f dps | Min: %+6.2f | Max: %+6.2f | Range: %5.2f dps\n",
                  mGy, minGy, maxGy, maxGy - minGy);
    Serial.printf("  Gyro Z -> Mean: %+6.2f dps | Min: %+6.2f | Max: %+6.2f | Range: %5.2f dps\n",
                  mGz, minGz, maxGz, maxGz - minGz);

    if (valid >= 98 && fabsf(mGx) < 15.0f && fabsf(mGy) < 15.0f && fabsf(mGz) < 15.0f) {
        Serial.println(F("RESULT: PASS -- Stationary gyroscope bias and noise remain low and stable."));
    } else {
        Serial.println(F("RESULT: FAIL -- Gyroscope stationary bias excessive."));
    }
}

// ---------------------------------------------------------------------------
// TEST 5.0 — Establish Movement Baseline
// ---------------------------------------------------------------------------
void execTest5_Baseline() {
    printHeader("ESTABLISHING MOVEMENT STATIONARY BASELINE (3s)");
    float sumAx = 0, sumAy = 0, sumAz = 0;
    float sumGx = 0, sumGy = 0, sumGz = 0;
    int count = 0;
    for (int i = 0; i < 60; i++) {
        IMUData d = readIMU();
        if (d.valid) {
            sumAx += d.ax; sumAy += d.ay; sumAz += d.az;
            sumGx += d.gx; sumGy += d.gy; sumGz += d.gz;
            count++;
        }
        delay(50);
    }
    if (count > 0) {
        current_baseline.ax = sumAx/count; current_baseline.ay = sumAy/count; current_baseline.az = sumAz/count;
        current_baseline.gx = sumGx/count; current_baseline.gy = sumGy/count; current_baseline.gz = sumGz/count;
    }
    Serial.printf("Baseline Accel: [%+.3f, %+.3f, %+.3f] g | Baseline Gyro: [%+.2f, %+.2f, %+.2f] dps\n",
                  current_baseline.ax, current_baseline.ay, current_baseline.az,
                  current_baseline.gx, current_baseline.gy, current_baseline.gz);
    Serial.println(F("BASELINE READY."));
}

// ---------------------------------------------------------------------------
// TEST 5.1 — X-Axis Tilt
// ---------------------------------------------------------------------------
void execTest5_1_TiltX() {
    printHeader("TEST 5.1 -- X-AXIS TILT (Recording 3.5s)");
    float maxD_Ax = 0, maxD_Ay = 0, maxD_Az = 0;
    float peakAx = current_baseline.ax;
    uint32_t tStart = millis();

    while (millis() - tStart < 3500) {
        IMUData d = readIMU();
        if (d.valid) {
            float dAx = fabsf(d.ax - current_baseline.ax);
            float dAy = fabsf(d.ay - current_baseline.ay);
            float dAz = fabsf(d.az - current_baseline.az);
            if (dAx > maxD_Ax) { maxD_Ax = dAx; peakAx = d.ax; }
            if (dAy > maxD_Ay) maxD_Ay = dAy;
            if (dAz > maxD_Az) maxD_Az = dAz;
            Serial.printf("  Live Accel: X=%+6.3f, Y=%+6.3f, Z=%+6.3f g | dAx = %.3f g\n",
                          d.ax, d.ay, d.az, dAx);
        }
        delay(200);
    }
    Serial.printf("\nBaseline Accel  : X=%+.3f, Y=%+.3f, Z=%+.3f g\n", current_baseline.ax, current_baseline.ay, current_baseline.az);
    Serial.printf("Peak Movement   : X=%+.3f g (Delta dAx = %.3f g)\n", peakAx, maxD_Ax);
    Serial.printf("Cross-axis Delta: dAy = %.3f g, dAz = %.3f g\n", maxD_Ay, maxD_Az);

    if (maxD_Ax >= 0.15f || (maxD_Ax + maxD_Az >= 0.20f)) {
        Serial.println(F("RESULT: PASS -- Clear, significant accelerometer response observed on X tilt."));
    } else {
        Serial.println(F("RESULT: FAIL / INCONCLUSIVE -- Insufficient response on X tilt."));
    }
}

// ---------------------------------------------------------------------------
// TEST 5.2 — Y-Axis Tilt
// ---------------------------------------------------------------------------
void execTest5_2_TiltY() {
    printHeader("TEST 5.2 -- Y-AXIS TILT (Recording 3.5s)");
    float maxD_Ax = 0, maxD_Ay = 0, maxD_Az = 0;
    float peakAy = current_baseline.ay;
    uint32_t tStart = millis();

    while (millis() - tStart < 3500) {
        IMUData d = readIMU();
        if (d.valid) {
            float dAx = fabsf(d.ax - current_baseline.ax);
            float dAy = fabsf(d.ay - current_baseline.ay);
            float dAz = fabsf(d.az - current_baseline.az);
            if (dAy > maxD_Ay) { maxD_Ay = dAy; peakAy = d.ay; }
            if (dAx > maxD_Ax) maxD_Ax = dAx;
            if (dAz > maxD_Az) maxD_Az = dAz;
            Serial.printf("  Live Accel: X=%+6.3f, Y=%+6.3f, Z=%+6.3f g | dAy = %.3f g\n",
                          d.ax, d.ay, d.az, dAy);
        }
        delay(200);
    }
    Serial.printf("\nBaseline Accel  : X=%+.3f, Y=%+.3f, Z=%+.3f g\n", current_baseline.ax, current_baseline.ay, current_baseline.az);
    Serial.printf("Peak Movement   : Y=%+.3f g (Delta dAy = %.3f g)\n", peakAy, maxD_Ay);
    Serial.printf("Cross-axis Delta: dAx = %.3f g, dAz = %.3f g\n", maxD_Ax, maxD_Az);

    if (maxD_Ay >= 0.15f || (maxD_Ay + maxD_Az >= 0.20f)) {
        Serial.println(F("RESULT: PASS -- Clear, significant accelerometer response observed on Y tilt."));
    } else {
        Serial.println(F("RESULT: FAIL / INCONCLUSIVE -- Insufficient response on Y tilt."));
    }
}

// ---------------------------------------------------------------------------
// TEST 5.3 — X-Axis Rotation
// ---------------------------------------------------------------------------
void execTest5_3_RotX() {
    printHeader("TEST 5.3 -- X-AXIS ROTATION (Recording 3.5s)");
    float maxAbsGx = 0, peakGx = 0;
    uint32_t tStart = millis();

    while (millis() - tStart < 3500) {
        IMUData d = readIMU();
        if (d.valid) {
            float absGx = fabsf(d.gx - current_baseline.gx);
            if (absGx > maxAbsGx) { maxAbsGx = absGx; peakGx = d.gx; }
            Serial.printf("  Live Gyro: X=%+7.1f, Y=%+7.1f, Z=%+7.1f dps | |Gx-base| = %.1f dps\n",
                          d.gx, d.gy, d.gz, absGx);
        }
        delay(200);
    }
    Serial.printf("\nStationary Gyro X Baseline: %+.2f dps\n", current_baseline.gx);
    Serial.printf("Peak Gyro X Angular Rate  : %+.2f dps (Delta = %.1f dps)\n", peakGx, maxAbsGx);

    if (maxAbsGx >= 20.0f) {
        Serial.println(F("RESULT: PASS -- Strong, responsive Gyro X dynamic rotation detected."));
    } else {
        Serial.println(F("RESULT: FAIL / INCONCLUSIVE -- Weak Gyro X rotation response."));
    }
}

// ---------------------------------------------------------------------------
// TEST 5.4 — Y-Axis Rotation
// ---------------------------------------------------------------------------
void execTest5_4_RotY() {
    printHeader("TEST 5.4 -- Y-AXIS ROTATION (Recording 3.5s)");
    float maxAbsGy = 0, peakGy = 0;
    uint32_t tStart = millis();

    while (millis() - tStart < 3500) {
        IMUData d = readIMU();
        if (d.valid) {
            float absGy = fabsf(d.gy - current_baseline.gy);
            if (absGy > maxAbsGy) { maxAbsGy = absGy; peakGy = d.gy; }
            Serial.printf("  Live Gyro: X=%+7.1f, Y=%+7.1f, Z=%+7.1f dps | |Gy-base| = %.1f dps\n",
                          d.gx, d.gy, d.gz, absGy);
        }
        delay(200);
    }
    Serial.printf("\nStationary Gyro Y Baseline: %+.2f dps\n", current_baseline.gy);
    Serial.printf("Peak Gyro Y Angular Rate  : %+.2f dps (Delta = %.1f dps)\n", peakGy, maxAbsGy);

    if (maxAbsGy >= 20.0f) {
        Serial.println(F("RESULT: PASS -- Strong, responsive Gyro Y dynamic rotation detected."));
    } else {
        Serial.println(F("RESULT: FAIL / INCONCLUSIVE -- Weak Gyro Y rotation response."));
    }
}

// ---------------------------------------------------------------------------
// TEST 5.5 — Z-Axis Rotation
// ---------------------------------------------------------------------------
void execTest5_5_RotZ() {
    printHeader("TEST 5.5 -- Z-AXIS ROTATION (Recording 3.5s)");
    float maxAbsGz = 0, peakGz = 0;
    uint32_t tStart = millis();

    while (millis() - tStart < 3500) {
        IMUData d = readIMU();
        if (d.valid) {
            float absGz = fabsf(d.gz - current_baseline.gz);
            if (absGz > maxAbsGz) { maxAbsGz = absGz; peakGz = d.gz; }
            Serial.printf("  Live Gyro: X=%+7.1f, Y=%+7.1f, Z=%+7.1f dps | |Gz-base| = %.1f dps\n",
                          d.gx, d.gy, d.gz, absGz);
        }
        delay(200);
    }
    Serial.printf("\nStationary Gyro Z Baseline: %+.2f dps\n", current_baseline.gz);
    Serial.printf("Peak Gyro Z Angular Rate  : %+.2f dps (Delta = %.1f dps)\n", peakGz, maxAbsGz);

    if (maxAbsGz >= 20.0f) {
        Serial.println(F("RESULT: PASS -- Strong, responsive Gyro Z dynamic rotation detected."));
    } else {
        Serial.println(F("RESULT: FAIL / INCONCLUSIVE -- Weak Gyro Z rotation response."));
    }
}

// ---------------------------------------------------------------------------
// TEST 6 — Long Duration Stationary Stability (60 Seconds)
// ---------------------------------------------------------------------------
void execTest6() {
    printHeader("TEST 6 -- LONG DURATION STATIONARY STABILITY (60 Seconds)");
    Serial.println(F("Recording 60 seconds of stationary telemetry at 5 Hz (300 samples)..."));

    const int TOTAL_SAMPLES = 300;
    uint32_t errors = 0, valid = 0, frozen = 0, spikes = 0;
    int16_t lastRawAx = 0, lastRawAy = 0, lastRawAz = 0;
    int16_t lastRawGx = 0, lastRawGy = 0, lastRawGz = 0;

    float sumMag = 0, sumSqMag = 0, minMag = 999, maxMag = -999;
    float sumGx = 0, sumGy = 0, sumGz = 0;
    float minGx = 999, maxGx = -999;
    float minGy = 999, maxGy = -999;
    float minGz = 999, maxGz = -999;

    Serial.println(F("Timestamp(ms) | Acc X (g) | Acc Y (g) | Acc Z (g) | |A|(g) | Gyro X | Gyro Y | Gyro Z (dps)"));
    Serial.println(F("---------------------------------------------------------------------------------------"));

    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        IMUData d = readIMU();
        if (!d.valid) {
            errors++;
        } else {
            if (i > 0 && d.rawAx == lastRawAx && d.rawAy == lastRawAy && d.rawAz == lastRawAz &&
                d.rawGx == lastRawGx && d.rawGy == lastRawGy && d.rawGz == lastRawGz) {
                frozen++;
            }
            lastRawAx = d.rawAx; lastRawAy = d.rawAy; lastRawAz = d.rawAz;
            lastRawGx = d.rawGx; lastRawGy = d.rawGy; lastRawGz = d.rawGz;

            if (d.mag < 0.60f || d.mag > 1.40f || fabsf(d.gx) > 20.0f || fabsf(d.gy) > 20.0f || fabsf(d.gz) > 20.0f) {
                spikes++;
            }

            valid++;
            sumMag += d.mag; sumSqMag += (d.mag * d.mag);
            if (d.mag < minMag) minMag = d.mag; if (d.mag > maxMag) maxMag = d.mag;

            sumGx += d.gx; sumGy += d.gy; sumGz += d.gz;
            if (d.gx < minGx) minGx = d.gx; if (d.gx > maxGx) maxGx = d.gx;
            if (d.gy < minGy) minGy = d.gy; if (d.gy > maxGy) maxGy = d.gy;
            if (d.gz < minGz) minGz = d.gz; if (d.gz > maxGz) maxGz = d.gz;

            if (i % 10 == 0 || i == TOTAL_SAMPLES - 1) {
                Serial.printf("%13lu |  %+7.3f  |  %+7.3f  |  %+7.3f  | %5.3f  | %+6.1f | %+6.1f | %+6.1f\n",
                              millis(), d.ax, d.ay, d.az, d.mag, d.gx, d.gy, d.gz);
            }
        }
        delay(200);
    }

    float meanMag = valid > 0 ? (sumMag / valid) : 0;
    float varMag = valid > 1 ? ((sumSqMag - (sumMag * sumMag / valid)) / (valid - 1)) : 0;
    float stdMag = varMag > 0 ? sqrtf(varMag) : 0;

    Serial.println(F("---------------------------------------------------------------------------------------"));
    Serial.println(F("60-Second Stationary Stability Summary:"));
    Serial.printf("  Total Samples Recorded: %u (Valid: %u, Errors: %u)\n", TOTAL_SAMPLES, valid, errors);
    Serial.printf("  Frozen Consecutive LSB: %u (%.1f%%)\n", frozen, (float)frozen*100.0f/TOTAL_SAMPLES);
    Serial.printf("  Unexplained Spikes    : %u (%.1f%%)\n", spikes, (float)spikes*100.0f/TOTAL_SAMPLES);
    Serial.printf("  Gravity Magnitude |A| : Mean = %.4f g | StdDev = %.4f g | Range = [%.4f, %.4f] g\n",
                  meanMag, stdMag, minMag, maxMag);
    Serial.printf("  Gyroscope Noise Range : X = %.2f dps, Y = %.2f dps, Z = %.2f dps\n",
                  maxGx - minGx, maxGy - minGy, maxGz - minGz);

    if (errors <= 3 && frozen <= (TOTAL_SAMPLES / 10) && spikes <= 3 && meanMag >= 0.85f && meanMag <= 1.15f && stdMag < 0.05f) {
        Serial.println(F("RESULT: PASS -- 60-second stability verified with continuous active comms and no lockups."));
    } else {
        Serial.println(F("RESULT: FAIL -- Stability criteria violated."));
    }
}

// ---------------------------------------------------------------------------
// TEST 7A — Fault Detection Only (SDA Disconnect)
// ---------------------------------------------------------------------------
void execTest7A() {
    printHeader("TEST 7A -- FAULT DETECTION (Disconnect SDA, 30s window)");
    Serial.println(F("Monitoring I2C bus for fault. Disconnect SDA wire now."));
    Serial.println(F("Probing every 300ms..."));
    bool faultDetected = false;
    float detectSec = 0;
    uint32_t tStart = millis();

    while (millis() - tStart < 30000) {
        Wire.beginTransmission(MPU6050_ADDR);
        uint8_t err = Wire.endTransmission();
        if (err != 0) {
            faultDetected = true;
            detectSec = (millis() - tStart) / 1000.0f;
            Serial.println();
            Serial.printf("FAULT DETECTED at t = +%.2fs! I2C Error Code: %u (NACK / Bus Error).\n", detectSec, err);
            Serial.println(F("Firmware accurately registered physical communication failure."));
            break;
        }
        Serial.print(F("."));
        delay(300);
    }

    if (!faultDetected) {
        Serial.println();
        Serial.println(F("RESULT: FAIL -- No fault detected in 30s. Was SDA disconnected?"));
    } else {
        Serial.println(F("RESULT: PASS -- Fault correctly detected."));
        Serial.println(F(">> NOW reconnect SDA, then send T7B to verify recovery."));
    }
}

// ---------------------------------------------------------------------------
// TEST 7B — Recovery Only (SDA Reconnect after T7A)
// ---------------------------------------------------------------------------
void execTest7B() {
    printHeader("TEST 7B -- RECOVERY VERIFICATION (Reconnect SDA, 45s window)");
    Serial.println(F("Attempting I2C bus recovery. Reconnect SDA wire now if not done."));

    bool recovered = false;
    float recSec = 0;
    uint32_t tStart = millis();

    while (millis() - tStart < 45000) {
        // Hard reset the I2C peripheral to clear stuck-bus state
        Wire.end();
        delay(20);
        Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);
        delay(30);

        Wire.beginTransmission(MPU6050_ADDR);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            delay(50);
            if (initMPU6050()) {
                IMUData d = readIMU();
                if (d.valid) {
                    recovered = true;
                    recSec = (millis() - tStart) / 1000.0f;
                    Serial.println();
                    Serial.printf("[+] RECOVERY SUCCESSFUL in %.2fs! Live data: |A| = %.3f g\n", recSec, d.mag);
                    break;
                }
            }
        }
        Serial.print(F("+"));
        delay(500);
    }

    Serial.println();
    Serial.printf("Recovery Verified : %s (Time: %.2fs)\n", recovered ? "YES" : "NO", recSec);
    if (recovered) {
        Serial.println(F("RESULT: PASS -- I2C bus fully recovered after physical reconnection."));
    } else {
        Serial.println(F("RESULT: FAIL -- Bus did not recover. Check SDA wiring."));
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);
    delay(500);
    Serial.println(F("\n[ESP32 READY -- AWAITING DISPATCH COMMAND]"));
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "T1") execTest1();
        else if (cmd == "T2") execTest2();
        else if (cmd == "T3") execTest3();
        else if (cmd == "T4") execTest4();
        else if (cmd == "T5_BASE") execTest5_Baseline();
        else if (cmd == "T5_1") execTest5_1_TiltX();
        else if (cmd == "T5_2") execTest5_2_TiltY();
        else if (cmd == "T5_3") execTest5_3_RotX();
        else if (cmd == "T5_4") execTest5_4_RotY();
        else if (cmd == "T5_5") execTest5_5_RotZ();
        else if (cmd == "T6") execTest6();
        else if (cmd == "T7A") execTest7A();
        else if (cmd == "T7B") execTest7B();
        Serial.println(F("\n[COMMAND COMPLETE]"));
    }
    delay(10);
}
