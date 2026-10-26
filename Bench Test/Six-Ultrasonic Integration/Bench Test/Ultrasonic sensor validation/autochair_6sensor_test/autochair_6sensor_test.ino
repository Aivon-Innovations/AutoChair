/*
  ============================================================
  AutoChair — Six-Sensor Obstacle Zone Detection
  ============================================================
  Sensors → ESP32 GPIO map:
    S1: TRIG=4,  ECHO=34   S4: TRIG=19, ECHO=39
    S2: TRIG=5,  ECHO=35   S5: TRIG=23, ECHO=32
    S3: TRIG=18, ECHO=36   S6: TRIG=13, ECHO=33

  Zone thresholds (PROVISIONAL):
    OBSTACLE  : < 400 mm  (< 40 cm)
    CAUTION   : 400–799 mm (40–79 cm)
    CLEAR     : >= 800 mm (>= 80 cm)
    NO SIGNAL : sensor timeout

  Serial Monitor: 115200 baud

  SAFETY NOTE: Detection/classification only.
  No motor commands are issued by this sketch.
  ============================================================
*/

// ---------- Pin map ----------
const uint8_t TRIG[6] = {  4,  5, 18, 19, 23, 13 };
const uint8_t ECHO[6] = { 34, 35, 36, 39, 32, 33 };

// ---------- Thresholds (mm) ----------
const float OBSTACLE_MM = 400.0f;   // < 40 cm → OBSTACLE
const float CAUTION_MM  = 800.0f;   // < 80 cm → CAUTION, else CLEAR

// ---------- Timing ----------
const uint32_t TIMEOUT_US        = 30000;  // 5.1 m max range
const uint32_t INTER_SENSOR_MS   = 60;    // gap between sensors (crosstalk)
const float    SOUND_MM_PER_US   = 0.3430f;

// ---------- Stats ----------
struct Stats { uint32_t ok = 0, timeout = 0; };
Stats stats[6];
uint32_t cycleNum = 0;

// =============================================================
// Measure one sensor → returns distance in mm, or -1 on timeout/noise
// HC-SR04 blind zone: anything below 20 mm is electrical noise.
// =============================================================
static constexpr float MIN_VALID_MM = 20.0f;
static constexpr float MAX_VALID_MM = 4000.0f;

float measure(uint8_t idx) {
  digitalWrite(TRIG[idx], LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG[idx], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG[idx], LOW);

  uint32_t dur = pulseIn(ECHO[idx], HIGH, TIMEOUT_US);
  if (dur == 0) return -1.0f;                          // timeout

  float dist = (dur * SOUND_MM_PER_US) / 2.0f;
  if (dist < MIN_VALID_MM) return -1.0f;               // below blind zone → noise
  if (dist > MAX_VALID_MM) return -1.0f;               // above max range → ignore
  return dist;
}

// =============================================================
// Zone string
// =============================================================
const char* zone(float dist_mm) {
  if (dist_mm < 0)               return "NO SIGNAL";
  if (dist_mm < OBSTACLE_MM)     return "** OBSTACLE **";
  if (dist_mm < CAUTION_MM)      return "  CAUTION    ";
  return                                "  CLEAR      ";
}

// =============================================================
// Setup
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  for (uint8_t i = 0; i < 6; i++) {
    pinMode(TRIG[i], OUTPUT);
    digitalWrite(TRIG[i], LOW);
    pinMode(ECHO[i], INPUT);
  }

  Serial.println("\n============================================================");
  Serial.println(" AutoChair — Six-Sensor Obstacle Zone Detection");
  Serial.println(" OBSTACLE < 400 mm  |  CAUTION < 800 mm  |  CLEAR >= 800 mm");
  Serial.println("============================================================\n");
  delay(1000);
}

// =============================================================
// Loop — one full 6-sensor cycle per iteration
// =============================================================
void loop() {
  cycleNum++;

  float  dist[6];
  bool   anyObstacle = false;
  bool   anyCaution  = false;
  bool   anyFault    = false;

  // Measure all 6 sensors
  for (uint8_t i = 0; i < 6; i++) {
    dist[i] = measure(i);
    if (dist[i] < 0)                         { anyFault    = true; stats[i].timeout++; }
    else if (dist[i] < OBSTACLE_MM)          { anyObstacle = true; stats[i].ok++;      }
    else if (dist[i] < CAUTION_MM)           { anyCaution  = true; stats[i].ok++;      }
    else                                     {                      stats[i].ok++;      }
    delay(INTER_SENSOR_MS);
  }

  // Overall zone
  const char* overall = anyObstacle ? "** OBSTACLE **"
                       : anyCaution  ? "  CAUTION    "
                       : anyFault    ? "  FAULT      "
                       :               "  CLEAR      ";

  // Print cycle header
  Serial.print("\n[CYCLE ");
  Serial.print(cycleNum);
  Serial.println("] ─────────────────────────────────────────────────");

  // Per-sensor lines
  for (uint8_t i = 0; i < 6; i++) {
    Serial.print("  [S");
    Serial.print(i + 1);
    Serial.print("] ");

    if (dist[i] < 0) {
      Serial.print("NO SIGNAL       ");
    } else {
      Serial.print(dist[i], 0);
      Serial.print(" mm  ");
      if (dist[i] < 100)  Serial.print("  ");
      if (dist[i] < 1000) Serial.print(" ");
    }

    Serial.println(zone(dist[i]));
  }

  // Overall
  Serial.print("  [OVERALL] ");
  Serial.println(overall);

  // Every 10 cycles print a summary
  if (cycleNum % 10 == 0) {
    Serial.println("\n  --- 10-Cycle Summary ---");
    for (uint8_t i = 0; i < 6; i++) {
      Serial.print("  S");
      Serial.print(i + 1);
      Serial.print(": OK=");
      Serial.print(stats[i].ok);
      Serial.print("  TIMEOUT=");
      Serial.println(stats[i].timeout);
    }
  }

  delay(200);
}
