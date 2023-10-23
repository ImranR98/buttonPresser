const int TOGGLE_BUTTON_PIN = D5;
const int BUTTON_DEBOUNCE_DELAY = 500;
unsigned long LAST_BUTTON_PUSH_TIME = 0;

const int SERVO_PIN = D3;
const int SERVO_ANGLE_MIN = 0;
const int SERVO_ANGLE_MAX = 188;
const int SERVO_PWM_MIN = 500;
const int SERVO_PWM_MAX = 2500;

const int PRESS_INTERVAL_SECONDS = 3600;
int CURRENT_WAIT_SECOND = 0;
const int SWITCH_NORMAL_POS = 115;
const int SWITCH_PRESS_POS = 87;

void setRGB(int redValue, int greenValue, int blueValue) {
  analogWrite(LED_RED, 255 - redValue);
  analogWrite(LED_GREEN, 255 - greenValue);
  analogWrite(LED_BLUE, 255 - blueValue);
}

void turnOffRGB() { setRGB(0, 0, 0); }

// Helper to simplify servo movements
void moveServoToApproxAngle(int angle) {
  Serial.println("Moving to angle: " + String(angle));
  int pulseWidth = map(angle, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX, SERVO_PWM_MIN,
                       SERVO_PWM_MAX);
  for (int i = 0; i < 3;
       i++) { // Large angle changes need more than one "signal"
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(SERVO_PIN, LOW);
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  moveServoToApproxAngle(SWITCH_NORMAL_POS);

  delay(5000); // For debugging - give me time to switch to serial monitor
}

void pressButton() {
  moveServoToApproxAngle(SWITCH_PRESS_POS);
  delay(30);
  moveServoToApproxAngle(SWITCH_NORMAL_POS);
  delay(30);
  moveServoToApproxAngle(SWITCH_PRESS_POS);
}

void loop() {
  if ((millis() - LAST_BUTTON_PUSH_TIME) > BUTTON_DEBOUNCE_DELAY) {
    if (!digitalRead(TOGGLE_BUTTON_PIN)) {
      LAST_BUTTON_PUSH_TIME = millis();
      pressButton();
    }
  }
  if (CURRENT_WAIT_SECOND >= PRESS_INTERVAL_SECONDS) {
    CURRENT_WAIT_SECOND = 0;
    pressButton();
  }
  CURRENT_WAIT_SECOND++;
  Serial.println(CURRENT_WAIT_SECOND);
  delay(1000);
}
