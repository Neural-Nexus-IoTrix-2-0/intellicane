import os
import time
from typing import Optional
from fastapi import FastAPI, File, UploadFile, Query, HTTPException
from fastapi.responses import HTMLResponse, FileResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware
import requests

from .analyzer import SceneAnalyzer
from .tts_engine import TTSEngine
from .schemas import BlindAssistanceResult

app = FastAPI(
    title="IntelliCane AI Voice & Vision Service",
    description="Multimodal scene understanding and audio guidance service for blind users",
    version="1.0.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

analyzer = SceneAnalyzer()
tts = TTSEngine()


@app.get("/health")
def health():
    return {
        "status": "healthy",
        "service": "IntelliCane AI Voice Service",
        "gemini_configured": bool(analyzer.api_key and analyzer.api_key != "YOUR_GEMINI_API_KEY"),
        "tts_available": True
    }


@app.post("/analyze/image", response_model=BlindAssistanceResult)
async def analyze_image(
    file: UploadFile = File(...),
    speak: bool = Query(False, description="Speak result aloud on server speakers"),
    api_key: Optional[str] = Query(None, description="Optional Google Gemini API key")
):
    """Analyze an uploaded camera frame and return blind assistance navigation details."""
    image_bytes = await file.read()
    if not image_bytes:
        raise HTTPException(status_code=400, detail="Empty image uploaded")

    result = analyzer.analyze(image_bytes, api_key=api_key)

    # Synthesize audio file
    audio_path = tts.synthesize_to_file(result.spoken_summary)
    if audio_path:
        filename = os.path.basename(audio_path)
        result.audio_url = f"/audio/{filename}"

    if speak:
        tts.speak_now(result.spoken_summary)

    return result


@app.get("/analyze/cane", response_model=BlindAssistanceResult)
def analyze_from_cane(
    cane_url: str = Query("http://192.168.4.1/capture", description="Full URL to ESP32-CAM capture endpoint"),
    speak: bool = Query(True, description="Speak description aloud via host speaker"),
    api_key: Optional[str] = Query(None, description="Optional Gemini API key")
):
    """Fetch the latest frame from the ESP32-CAM, analyze hazards and objects, and speak aloud."""
    try:
        resp = requests.get(cane_url, timeout=6)
        resp.raise_for_status()
        image_bytes = resp.content
    except Exception as e:
        raise HTTPException(
            status_code=502,
            detail=f"Failed to fetch image from ESP32-CAM at {cane_url}: {str(e)}"
        )

    result = analyzer.analyze(image_bytes, api_key=api_key)

    audio_path = tts.synthesize_to_file(result.spoken_summary)
    if audio_path:
        filename = os.path.basename(audio_path)
        result.audio_url = f"/audio/{filename}"

    if speak:
        tts.speak_now(result.spoken_summary)

    return result


@app.get("/audio/{filename}")
def get_audio(filename: str):
    """Serve synthesized audio guidance files."""
    file_path = os.path.join(tts.output_dir, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="Audio file not found")
    media_type = "audio/aiff" if filename.endswith(".aiff") else "audio/mpeg"
    return FileResponse(file_path, media_type=media_type)


@app.get("/", response_class=HTMLResponse)
def index():
    """Interactive Web Dashboard for demonstration and testing."""
    html_content = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>IntelliCane AI Voice Service</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; }
    .container { max-width: 680px; margin: 0 auto; }
    h1 { color: #38bdf8; margin-bottom: 4px; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 18px; margin-bottom: 16px; }
    .btn { background: #38bdf8; color: #0f172a; padding: 12px 20px; border: none; border-radius: 8px; font-weight: bold; cursor: pointer; font-size: 1rem; }
    .btn:hover { background: #0284c7; }
    input[type=text] { width: 100%; box-sizing: border-box; padding: 10px; background: #0f172a; border: 1px solid #334155; color: white; border-radius: 6px; margin-bottom: 10px; }
    .hazard-badge { display: inline-block; padding: 4px 10px; border-radius: 6px; font-weight: bold; margin-top: 6px; }
    .hazard-danger { background: #ef4444; color: white; }
    .hazard-ok { background: #10b981; color: white; }
    .speech-box { background: rgba(56, 189, 248, 0.1); border-left: 4px solid #38bdf8; padding: 12px; border-radius: 4px; font-size: 1.1rem; line-height: 1.5; margin: 10px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>IntelliCane AI Voice Service</h1>
    <p style="color: #94a3b8;">Multimodal Scene Analysis & Speech Synthesis for Blind Navigation</p>

    <div class="card">
      <h3>Query ESP32-CAM</h3>
      <label style="font-size:0.85rem; color:#94a3b8;">ESP32-CAM Capture URL:</label>
      <input type="text" id="caneUrl" value="http://192.168.4.1/capture">
      <button class="btn" onclick="queryCane()">Trigger Cane Snapshot & Analyze</button>
    </div>

    <div class="card" id="resultCard" style="display:none;">
      <h3>Analysis Result</h3>
      <div id="speechBox" class="speech-box"></div>
      <div id="hazardBox"></div>
      <p><strong>Walkable Path:</strong> <span id="pathText" style="color:#38bdf8;"></span></p>
      <p><strong>Signs / Text:</strong> <span id="signText"></span></p>
      <p><strong>Latency:</strong> <span id="latencyText"></span> ms</p>
      <audio id="audioPlayer" controls style="width:100%; margin-top:10px; display:none;"></audio>
    </div>
  </div>

  <script>
    async function queryCane() {
      const url = document.getElementById('caneUrl').value;
      const resCard = document.getElementById('resultCard');
      resCard.style.display = 'block';
      document.getElementById('speechBox').innerText = 'Fetching camera snapshot and querying AI...';

      try {
        const resp = await fetch('/analyze/cane?cane_url=' + encodeURIComponent(url) + '&speak=true');
        const data = await resp.json();

        document.getElementById('speechBox').innerText = data.spoken_summary;
        document.getElementById('pathText').innerText = data.walkable_path;
        document.getElementById('signText').innerText = data.text_and_signs;
        document.getElementById('latencyText').innerText = data.latency_ms;

        const hBox = document.getElementById('hazardBox');
        if (data.urgent_hazards && data.urgent_hazards.length > 0 && data.urgent_hazards[0] !== 'None') {
          hBox.innerHTML = '<span class="hazard-badge hazard-danger">⚠️ Hazards: ' + data.urgent_hazards.join(', ') + '</span>';
        } else {
          hBox.innerHTML = '<span class="hazard-badge hazard-ok">✅ Clear: No urgent hazards</span>';
        }

        if (data.audio_url) {
          const audio = document.getElementById('audioPlayer');
          audio.src = data.audio_url;
          audio.style.display = 'block';
          audio.play();
        }
      } catch (err) {
        document.getElementById('speechBox').innerText = 'Error: ' + err.message;
      }
    }
  </script>
</body>
</html>"""
    return HTMLResponse(content=html_content)
