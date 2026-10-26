/*
  S6 RAW DIAGNOSTIC — No filters, shows exact pulse in microseconds
  TRIG = GPIO13,  ECHO = GPIO33
  Baud = 115200
*/

const uint8_t TRIG_PIN = 13;
const uint8_t ECHO_PIN = 33;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("\n=== S6 RAW DIAGNOSTIC ===");
  Serial.println("pulse=0      → NO echo reaching GPIO33 (wiring broken)");
  Serial.println("pulse~99     → 17mm internal coupling (R2→GND missing)");
  Serial.println("pulse>100    → REAL echo — sensor working!\n");
  delay(1000);
}

void loop() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  uint32_t pulse = pulseIn(ECHO_PIN, HIGH, 30000);
  float dist = (pulse * 0.3430f) / 2.0f;

  Serial.print("pulse=");
  Serial.print(pulse);
  Serial.print("us   dist=");
  Serial.print(dist, 0);
  Serial.print("mm   → ");

  if (pulse == 0)        Serial.println("NO ECHO — GPIO33 not receiving signal");
  else if (pulse < 120)  Serial.println("internal coupling only — R2→GND missing or wrong rail");
  else                   Serial.println("REAL ECHO ✓");

  delay(400);
}
