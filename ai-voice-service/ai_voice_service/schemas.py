from typing import List, Optional
from pydantic import BaseModel, Field
import time


class DetectedObject(BaseModel):
    name: str = Field(..., description="Name of the detected object (e.g., chair, person, stair)")
    position: str = Field(..., description="Clock-face position relative to the user (e.g., 12 o'clock, 2 o'clock)")
    distance: str = Field(..., description="Estimated distance (e.g., 1.5m, close <1m, far >3m)")
    hazard_level: str = Field(default="none", description="Hazard level: none, low, medium, high")


class BlindAssistanceResult(BaseModel):
    status: str = Field(default="success", description="Status code or message")
    spoken_summary: str = Field(
        ...,
        description="Natural, concise audio description (1-3 sentences) prioritizing urgent hazards, walkable path, then key surroundings."
    )
    urgent_hazards: List[str] = Field(
        default_factory=list,
        description="List of immediate physical hazards (drop-offs, steps, obstacles in line of movement, moving objects)"
    )
    walkable_path: str = Field(
        ...,
        description="Clear direction where the user can safely step or walk (e.g., 'Clear path forward at 12 o'clock')"
    )
    detected_objects: List[DetectedObject] = Field(
        default_factory=list,
        description="List of identified objects around the user with clock-face orientation and distance"
    )
    text_and_signs: str = Field(
        default="None",
        description="Important signage, room names/numbers, or navigational text visible in frame"
    )
    lighting_condition: str = Field(
        default="good",
        description="Lighting quality: good, dim, dark"
    )
    audio_url: Optional[str] = Field(
        default=None,
        description="URL or endpoint to fetch synthesized audio speech"
    )
    latency_ms: int = Field(
        default=0,
        description="Total processing latency in milliseconds"
    )
    timestamp: float = Field(
        default_factory=time.time,
        description="Unix timestamp of the inference"
    )
