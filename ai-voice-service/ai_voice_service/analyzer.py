import base64
import json
import os
import re
import time
from typing import Optional
import requests

from .schemas import BlindAssistanceResult, DetectedObject

SYSTEM_PROMPT = """
You are IntelliCane AI, an assistive vision system embedded on a smart white cane for a blind or visually impaired person.
Analyze the provided camera frame from the perspective of the user holding the white cane.

Crucial Guidelines:
1. Urgent Hazards: Identify immediate danger first! (Stairs/steps down, drop-offs, curbs, potholes, low-hanging tree branches/signs at head height, moving vehicles, approaching bicycles/people, or tripping hazards directly in the walking path).
2. Walkable Path: Clearly specify where it is safe to walk using clock-face directions (e.g., "Clear path ahead at 12 o'clock", "Path directly ahead is blocked, safe passage on your left at 10 o'clock").
3. Surrounding Objects: List prominent objects with their clock-face orientation relative to the user (e.g., 12 o'clock, 1 o'clock, 10 o'clock) and estimated distance (e.g., "0.8m", "close <1m", "medium 1-3m", "far >3m"), plus hazard level ("none", "low", "medium", "high").
4. Signs and Text: Note readable text on doors, room numbers, exit signs, street signs, or pedestrian crossing signals.
5. Spoken Summary: Exactly 1 to 3 clear, concise, calm sentences prioritizing hazards first, walkable direction second, and key surroundings third. This sentence will be spoken directly into the blind person's ear.

Respond strictly with a JSON object matching this schema:
{
  "spoken_summary": "1-3 concise sentences for audio playback",
  "urgent_hazards": ["string descriptions of immediate hazards, or empty if none"],
  "walkable_path": "string description of clear walking trajectory",
  "detected_objects": [
    {
      "name": "object name",
      "position": "clock position (e.g. 12 o'clock, 2 o'clock)",
      "distance": "estimated distance (e.g. 1.2m, <1m)",
      "hazard_level": "none | low | medium | high"
    }
  ],
  "text_and_signs": "any signs or navigational text, or 'None'",
  "lighting_condition": "good | dim | dark"
}
"""


class SceneAnalyzer:
    """Multimodal scene understanding engine for blind navigation."""

    def __init__(self, api_key: Optional[str] = None, model: str = "gemini-1.5-flash"):
        self.api_key = api_key or os.environ.get("GEMINI_API_KEY", "")
        self.model = model

    def analyze(self, image_bytes: bytes, api_key: Optional[str] = None) -> BlindAssistanceResult:
        start_time = time.time()
        active_key = api_key or self.api_key

        if active_key and active_key != "YOUR_GEMINI_API_KEY":
            return self._call_gemini(image_bytes, active_key, start_time)
        else:
            return self._bench_fallback(image_bytes, start_time)

    def _call_gemini(self, image_bytes: bytes, api_key: str, start_time: float) -> BlindAssistanceResult:
        b64_image = base64.b64encode(image_bytes).decode("utf-8")
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{self.model}:generateContent?key={api_key}"

        payload = {
            "contents": [
                {
                    "parts": [
                        {"text": SYSTEM_PROMPT},
                        {
                            "inlineData": {
                                "mimeType": "image/jpeg",
                                "data": b64_image
                            }
                        }
                    ]
                }
            ],
            "generationConfig": {
                "temperature": 0.2,
                "responseMimeType": "application/json"
            }
        }

        try:
            resp = requests.post(url, json=payload, timeout=20)
            resp.raise_for_status()
            data = resp.json()

            candidate_text = data["candidates"][0]["content"]["parts"][0]["text"]
            # Clean possible markdown formatting
            cleaned_text = re.sub(r"^```(?:json)?\s*", "", candidate_text.strip())
            cleaned_text = re.sub(r"\s*```$", "", cleaned_text.strip())

            parsed = json.loads(cleaned_text)

            detected_objects = [
                DetectedObject(
                    name=obj.get("name", "object"),
                    position=obj.get("position", "12 o'clock"),
                    distance=obj.get("distance", "medium"),
                    hazard_level=obj.get("hazard_level", "none")
                )
                for obj in parsed.get("detected_objects", [])
            ]

            latency_ms = int((time.time() - start_time) * 1000)

            return BlindAssistanceResult(
                status="success",
                spoken_summary=parsed.get("spoken_summary", "Environment analyzed. Path appears clear."),
                urgent_hazards=parsed.get("urgent_hazards", []),
                walkable_path=parsed.get("walkable_path", "Clear forward."),
                detected_objects=detected_objects,
                text_and_signs=parsed.get("text_and_signs", "None"),
                lighting_condition=parsed.get("lighting_condition", "good"),
                latency_ms=latency_ms
            )

        except Exception as e:
            # If API fails or network issue, fallback gracefully
            latency_ms = int((time.time() - start_time) * 1000)
            return BlindAssistanceResult(
                status="api_error",
                spoken_summary=f"Vision service notification: {str(e)}. Please rely on cane physical haptic feedback.",
                urgent_hazards=["Vision API offline - Rely on physical white cane haptics"],
                walkable_path="Caution advised. Step slowly.",
                detected_objects=[],
                text_and_signs="None",
                lighting_condition="unknown",
                latency_ms=latency_ms
            )

    def _bench_fallback(self, image_bytes: bytes, start_time: float) -> BlindAssistanceResult:
        """Realistic simulated response for bench testing and offline development."""
        latency_ms = int((time.time() - start_time) * 1000)
        img_len = len(image_bytes)

        return BlindAssistanceResult(
            status="bench_simulation",
            spoken_summary="Path straight ahead at 12 o'clock is clear for 2 meters. A low obstacle is at 2 o'clock on your right. Step forward with caution.",
            urgent_hazards=["Low obstacle at 2 o'clock (1.2m away)"],
            walkable_path="Clear corridor directly ahead at 12 o'clock for approximately 2 to 3 meters.",
            detected_objects=[
                DetectedObject(
                    name="doorway",
                    position="12 o'clock",
                    distance="2.5m",
                    hazard_level="none"
                ),
                DetectedObject(
                    name="chair/stool",
                    position="2 o'clock",
                    distance="1.2m",
                    hazard_level="medium"
                )
            ],
            text_and_signs="Office doorway visible straight ahead",
            lighting_condition="good",
            latency_ms=latency_ms
        )
