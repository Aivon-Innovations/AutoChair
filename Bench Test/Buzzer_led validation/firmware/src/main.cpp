#include <Arduino.h>
#include <rom/rtc.h>

#define BUZZER_PIN 33
#define LED_PIN 32

void playTone(int durationMs, int freqHz = 2700) {
    int halfPeriodUs = 1000000 / (freqHz * 2);
    long cycles = ((long)freqHz * durationMs) / 1000;
    for (long i = 0; i < cycles; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delayMicroseconds(halfPeriodUs);
        digitalWrite(BUZZER_PIN, LOW);
        delayMicroseconds(halfPeriodUs);
    }
}

void printSystemStatus() {
    Serial.println("--- SYSTEM TELEMETRY ---");
    Serial.print("Uptime_ms: "); Serial.println(millis());
    Serial.print("Free_Heap_bytes: "); Serial.println(esp_get_free_heap_size());
    Serial.print("CPU0_Reset_Reason: "); Serial.println(rtc_get_reset_reason(0));
    Serial.print("CPU1_Reset_Reason: "); Serial.println(rtc_get_reset_reason(1));
    Serial.println("------------------------");
}

void runLed20() {
    Serial.println("\n========================================================");
    Serial.println(">>> STEP B3: LED 20-CYCLE REPETITION TEST START <<<");
    Serial.println("Target: 20 cycles (1.0s ON / 1.0s OFF) on GPIO32");
    Serial.println("========================================================");

    digitalWrite(LED_PIN, LOW);
    delay(500);

    int successCount = 0;
    for (int cycle = 1; cycle <= 20; cycle++) {
        uint32_t t_start = millis();

        // 1. LED ON
        digitalWrite(LED_PIN, HIGH);
        Serial.print("[LED_TELEMETRY] cycle="); Serial.print(cycle);
        Serial.print("/20 state=ON gpio32=1 uptime_ms="); Serial.println(millis());
        delay(1000);

        // 2. LED OFF
        digitalWrite(LED_PIN, LOW);
        Serial.print("[LED_TELEMETRY] cycle="); Serial.print(cycle);
        Serial.print("/20 state=OFF gpio32=0 uptime_ms="); Serial.println(millis());
        delay(1000);

        uint32_t totalDuration = millis() - t_start;
        if (totalDuration >= 1950 && totalDuration <= 2100) {
            successCount++;
            Serial.print("[CYCLE_RESULT] cycle="); Serial.print(cycle);
            Serial.print(" status=PASS duration_ms="); Serial.println(totalDuration);
        } else {
            Serial.print("[CYCLE_RESULT] cycle="); Serial.print(cycle);
            Serial.print(" status=FAIL duration_ms="); Serial.println(totalDuration);
        }
    }

    Serial.println("========================================================");
    Serial.print("LED 20-CYCLE TEST RESULT: ");
    Serial.print(successCount); Serial.println("/20 SUCCESSFUL CYCLES");
    Serial.println("========================================================");
}

void runCombined20() {
    Serial.println("\n========================================================");
    Serial.println(">>> PART C: COMBINED BUZZER + LED 20-CYCLE TEST START <<<");
    Serial.println("Target: 20 cycles (Synchronized Buzzer + LED 1.0s ON / 1.0s OFF)");
    Serial.println("========================================================");

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    delay(500);

    int successCount = 0;
    for (int cycle = 1; cycle <= 20; cycle++) {
        uint32_t t_start = millis();

        // 1. BOTH ON
        digitalWrite(LED_PIN, HIGH);
        Serial.print("[COMBINED_TELEMETRY] cycle="); Serial.print(cycle);
        Serial.print("/20 buzzer=ON_2700Hz led=ON gpio33=PULSING gpio32=1 uptime_ms=");
        Serial.println(millis());
        playTone(1000, 2700);

        // 2. BOTH OFF
        digitalWrite(LED_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        Serial.print("[COMBINED_TELEMETRY] cycle="); Serial.print(cycle);
        Serial.print("/20 buzzer=OFF_SILENCE led=OFF gpio33=LOW gpio32=0 uptime_ms=");
        Serial.println(millis());
        delay(1000);

        uint32_t totalDuration = millis() - t_start;
        if (totalDuration >= 1950 && totalDuration <= 2100) {
            successCount++;
            Serial.print("[CYCLE_RESULT] cycle="); Serial.print(cycle);
            Serial.print(" status=PASS duration_ms="); Serial.println(totalDuration);
        } else {
            Serial.print("[CYCLE_RESULT] cycle="); Serial.print(cycle);
            Serial.print(" status=FAIL duration_ms="); Serial.println(totalDuration);
        }
    }

    Serial.println("========================================================");
    Serial.print("COMBINED 20-CYCLE TEST RESULT: ");
    Serial.print(successCount); Serial.println("/20 SUCCESSFUL CYCLES");
    Serial.println("========================================================");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    Serial.println("\n========================================================");
    Serial.println(" AUTOCHAIR BENCH VALIDATOR: BUZZER & LED CONTROL ENGINE ");
    Serial.println("========================================================");
    Serial.println("READY.");
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() == 0) return;

        if (cmd == "CMD:LED_20") {
            runLed20();
        } else if (cmd == "CMD:COMBINED_20") {
            runCombined20();
        } else if (cmd == "CMD:STATUS") {
            printSystemStatus();
        }
    }
}
