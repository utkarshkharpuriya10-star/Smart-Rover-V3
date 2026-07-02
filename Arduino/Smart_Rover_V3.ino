#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "Galaxy A35";
const char* password = "uday1234";

#define ENA 14
#define IN1 27
#define IN2 26
#define ENB 32
#define IN3 25
#define IN4 33

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 13
#define IR_LEFT 34
#define IR_RIGHT 35

WebServer server(80);
Servo scanServo;

int motorSpeed = 200;
enum Mode { MANUAL, AUTO_OBSTACLE, LINE_FOLLOW };
Mode currentMode = MANUAL;

const int pwmFreq = 5000;
const int pwmResBits = 8;

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // New ESP32 core 3.x LEDC API: attach directly to the pin, no channel needed
  ledcAttach(ENA, pwmFreq, pwmResBits);
  ledcAttach(ENB, pwmFreq, pwmResBits);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  scanServo.setPeriodHertz(50);
  scanServo.attach(SERVO_PIN, 500, 2400);
  scanServo.write(90);

  stopMotors();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Rover IP: ");
  Serial.println(WiFi.localIP());

  setupRoutes();
  server.begin();
}

void loop() {
  server.handleClient();
  if (currentMode == AUTO_OBSTACLE) autoObstacleLoop();
  else if (currentMode == LINE_FOLLOW) lineFollowLoop();
}

void setMotorA(int speed, bool forward) {
  digitalWrite(IN1, forward ? HIGH : LOW);
  digitalWrite(IN2, forward ? LOW : HIGH);
  ledcWrite(ENA, speed); // new API: write directly by pin
}

void setMotorB(int speed, bool forward) {
  digitalWrite(IN3, forward ? HIGH : LOW);
  digitalWrite(IN4, forward ? LOW : HIGH);
  ledcWrite(ENB, speed); // new API: write directly by pin
}

void moveForward(int speed) { setMotorA(speed, true); setMotorB(speed, true); }
void moveBackward(int speed) { setMotorA(speed, false); setMotorB(speed, false); }
void turnLeft(int speed) { setMotorA(speed, false); setMotorB(speed, true); }
void turnRight(int speed) { setMotorA(speed, true); setMotorB(speed, false); }
void stopMotors() { ledcWrite(ENA, 0); ledcWrite(ENB, 0); }

long getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 400;
  return duration * 0.0343 / 2;
}

void autoObstacleLoop() {
  long distance = getDistanceCM();
  if (distance > 25) {
    moveForward(motorSpeed);
  } else {
    stopMotors();
    delay(200);
    moveBackward(motorSpeed);
    delay(300);
    stopMotors();

    scanServo.write(150);
    delay(400);
    long leftDist = getDistanceCM();

    scanServo.write(30);
    delay(500);
    long rightDist = getDistanceCM();

    scanServo.write(90);
    delay(300);

    if (leftDist > rightDist) turnLeft(motorSpeed);
    else turnRight(motorSpeed);
    delay(400);
    stopMotors();
  }
}

void lineFollowLoop() {
  int leftVal = digitalRead(IR_LEFT);
  int rightVal = digitalRead(IR_RIGHT);
  if (leftVal == LOW && rightVal == LOW) moveForward(motorSpeed);
  else if (leftVal == HIGH && rightVal == LOW) turnLeft(motorSpeed);
  else if (leftVal == LOW && rightVal == HIGH) turnRight(motorSpeed);
  else stopMotors();
}

void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/mode", handleMode);
  server.on("/speed", handleSpeed);
  server.on("/status", handleStatus);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><title>AI Rover V3</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:Arial;text-align:center;background:#111;color:#eee;}
button{font-size:20px;padding:15px 25px;margin:8px;border-radius:10px;border:none;background:#2a7;color:white;}
.stop{background:#c33;} .mode{background:#37c;} h2{margin-top:30px;}</style></head>
<body><h1>AI Rover V3 Control</h1>
<h2>Movement</h2>
<div><button onclick="send('/move?dir=forward')">Forward</button></div>
<div><button onclick="send('/move?dir=left')">Left</button>
<button onclick="send('/move?dir=stop')" class="stop">Stop</button>
<button onclick="send('/move?dir=right')">Right</button></div>
<div><button onclick="send('/move?dir=backward')">Backward</button></div>
<h2>Speed</h2>
<input type="range" min="100" max="255" value="200" onchange="send('/speed?val='+this.value)">
<h2>Mode</h2>
<button class="mode" onclick="send('/mode?set=manual')">Manual</button>
<button class="mode" onclick="send('/mode?set=obstacle')">Auto Obstacle</button>
<button class="mode" onclick="send('/mode?set=line')">Line Follow</button>
<script>function send(url){fetch(url).catch(e=>console.log(e));}</script>
</body></html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleMove() {
  if (currentMode != MANUAL) { server.send(200, "text/plain", "Ignored"); return; }
  String dir = server.arg("dir");
  if (dir == "forward") moveForward(motorSpeed);
  else if (dir == "backward") moveBackward(motorSpeed);
  else if (dir == "left") turnLeft(motorSpeed);
  else if (dir == "right") turnRight(motorSpeed);
  else if (dir == "stop") stopMotors();
  server.send(200, "text/plain", "OK");
}

void handleMode() {
  String m = server.arg("set");
  stopMotors();
  if (m == "manual") currentMode = MANUAL;
  else if (m == "obstacle") currentMode = AUTO_OBSTACLE;
  else if (m == "line") currentMode = LINE_FOLLOW;
  server.send(200, "text/plain", "Mode: " + m);
}

void handleSpeed() {
  if (server.hasArg("val")) motorSpeed = constrain(server.arg("val").toInt(), 0, 255);
  server.send(200, "text/plain", "Speed: " + String(motorSpeed));
}

void handleStatus() {
  String status = "Mode: ";
  if (currentMode == MANUAL) status += "Manual";
  else if (currentMode == AUTO_OBSTACLE) status += "Obstacle Avoidance";
  else status += "Line Follow";
  status += " | Distance: " + String(getDistanceCM()) + "cm | Speed: " + String(motorSpeed);
  server.send(200, "text/plain", status);
}
