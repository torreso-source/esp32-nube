void setup() {
  Serial.begin(115200);
  pinMode(4, OUTPUT);   // LED flash del ESP32-CAM
}
void loop() {
  digitalWrite(4, HIGH);
  Serial.println("flash ON");
  delay(500);
  digitalWrite(4, LOW);
  Serial.println("flash OFF");
  delay(500);
}
