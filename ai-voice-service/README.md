# IntelliCane AI Voice Service

The AI Voice Service provides multimodal scene understanding, obstacle identification, and neural Text-to-Speech (TTS) audio feedback for the IntelliCane smart white cane.

---

## Capabilities

- **Blind Navigation Scene Understanding**:
  - Leverages Google Gemini Multimodal Vision API (`gemini-1.5-flash` or `gemini-2.0-flash`).
  - Prioritizes critical safety information:
    1. **Urgent Hazards**: Stairs/steps down, curbs, drop-offs, low-hanging obstacles, moving vehicles or people, tripping hazards.
    2. **Walkable Path**: Clear navigable directions formatted as clock-face orientations (e.g., 12 o'clock, 10 o'clock, 2 o'clock).
    3. **Surrounding Objects**: Object identification with estimated distances and hazard ratings.
    4. **Text & Signage**: Room numbers, doorway signs, exit indicators, transit symbols.
- **Audio Feedback / TTS**:
  - Offline local macOS `say` voice synthesis (sub-second latency, natural voice).
  - Neural TTS fallback via `edge-tts` / `gTTS`.
  - Speaks audio directly on host speakers or provides downloadable audio files.
- **REST Endpoints & Interactive Dashboard**:
  - `POST /analyze/image`: Upload raw image or multipart frame.
  - `GET /analyze/cane?cane_url=http://.../capture`: Pulls latest snapshot from the ESP32-CAM and performs live analysis with audio readout.
  - `GET /`: Interactive web test interface.

---

## Quick Start

### 1. Setup Environment
```sh
cd ai-voice-service
pip install -r requirements.txt
```

### 2. Run the Service
```sh
# Start FastAPI server on port 8000
python -m ai_voice_service.cli serve --port 8000
```
Open [http://localhost:8000](http://localhost:8000) in your browser to interact with the dashboard.

### 3. Command Line Testing

#### Query ESP32-CAM directly:
```sh
python -m ai_voice_service.cli cane --url http://192.168.4.1/capture
```

#### Run offline demo (with audio readout):
```sh
python -m ai_voice_service.cli demo
```

#### Analyze a local image:
```sh
python -m ai_voice_service.cli image /path/to/test.jpg --key YOUR_GEMINI_API_KEY
```

---

## Running Unit Tests
```sh
pytest tests/
```
