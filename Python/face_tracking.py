"""
AI Rover - Face Detection & Follow Controller (Simple Version)
Requires: pip install opencv-python mediapipe requests

Before running:
1. Make sure your laptop is connected to the same WiFi as the rover ("Galaxy A35")
2. Set ROVER_IP below to the IP printed in the Arduino Serial Monitor
"""

import cv2
import mediapipe as mp
import requests
import time

# ---- CONFIG ----
ROVER_IP = "10.193.216.125"   # <-- your rover's actual IP
BASE_URL = f"http://{ROVER_IP}"
FRAME_WIDTH = 640
FRAME_HEIGHT = 480
CENTER_TOLERANCE = 60       # pixels - how far off-center before turning
MIN_FACE_SIZE = 8000        # face too small (far away) -> move forward
MAX_FACE_SIZE = 30000       # face too big (close) -> stop / back up
COMMAND_COOLDOWN = 0.3      # seconds between commands to avoid spamming rover

# ---- SETUP ----
mp_face = mp.solutions.face_detection
face_detector = mp_face.FaceDetection(model_selection=0, min_detection_confidence=0.6)

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)

last_command = None
last_command_time = 0


def send_command(direction):
    """Send a movement command to the rover, avoiding spam repeats."""
    global last_command, last_command_time
    now = time.time()
    if direction == last_command and (now - last_command_time) < COMMAND_COOLDOWN:
        return
    try:
        requests.get(f"{BASE_URL}/move", params={"dir": direction}, timeout=0.5)
        last_command = direction
        last_command_time = now
    except requests.exceptions.RequestException as e:
        print(f"[WARN] Could not reach rover: {e}")


print("Starting face tracking. Press 'q' to quit.")
print(f"Connecting to rover at {BASE_URL}")

while True:
    ret, frame = cap.read()
    if not ret:
        print("Camera not found / failed to read frame.")
        break

    frame = cv2.flip(frame, 1)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = face_detector.process(rgb)

    h, w, _ = frame.shape
    frame_center_x = w // 2

    if results.detections:
        # Use the largest detected face
        detection = max(
            results.detections,
            key=lambda d: d.location_data.relative_bounding_box.width
            * d.location_data.relative_bounding_box.height,
        )
        bbox = detection.location_data.relative_bounding_box
        x = int(bbox.xmin * w)
        y = int(bbox.ymin * h)
        bw = int(bbox.width * w)
        bh = int(bbox.height * h)
        face_center_x = x + bw // 2
        face_area = bw * bh

        cv2.rectangle(frame, (x, y), (x + bw, y + bh), (0, 255, 0), 2)
        cv2.circle(frame, (face_center_x, y + bh // 2), 5, (0, 0, 255), -1)

        offset = face_center_x - frame_center_x

        if offset > CENTER_TOLERANCE:
            send_command("right")
            status = "Turning Right"
        elif offset < -CENTER_TOLERANCE:
            send_command("left")
            status = "Turning Left"
        elif face_area < MIN_FACE_SIZE:
            send_command("forward")
            status = "Moving Forward"
        elif face_area > MAX_FACE_SIZE:
            send_command("backward")
            status = "Backing Up"
        else:
            send_command("stop")
            status = "Face Centered - Stopped"

        cv2.putText(frame, status, (10, 30), cv2.FONT_HERSHEY_SIMPLEX,
                    0.7, (0, 255, 0), 2)
    else:
        send_command("stop")
        cv2.putText(frame, "No face detected", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

    cv2.line(frame, (frame_center_x, 0), (frame_center_x, h), (255, 255, 0), 1)
    cv2.imshow("AI Rover V3 - Face Tracking", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        send_command("stop")
        break

cap.release()
cv2.destroyAllWindows()q
