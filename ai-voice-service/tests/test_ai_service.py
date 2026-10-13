import pytest
from ai_voice_service.schemas import BlindAssistanceResult, DetectedObject
from ai_voice_service.analyzer import SceneAnalyzer
from ai_voice_service.tts_engine import TTSEngine


def test_schema_instantiation():
    obj = DetectedObject(name="bench", position="10 o'clock", distance="1.8m", hazard_level="low")
    assert obj.name == "bench"
    assert obj.position == "10 o'clock"

    res = BlindAssistanceResult(
        spoken_summary="All clear forward.",
        urgent_hazards=[],
        walkable_path="12 o'clock clear",
        detected_objects=[obj],
        text_and_signs="Exit",
        lighting_condition="good"
    )
    assert res.spoken_summary == "All clear forward."
    assert len(res.detected_objects) == 1
    assert res.detected_objects[0].name == "bench"


def test_scene_analyzer_bench_fallback():
    analyzer = SceneAnalyzer()
    dummy_bytes = b"\xff\xd8\xff\xe0testimagebytes"
    result = analyzer.analyze(dummy_bytes)

    assert result.status == "bench_simulation"
    assert len(result.spoken_summary) > 0
    assert len(result.detected_objects) >= 1
    assert "12 o'clock" in result.walkable_path or "ahead" in result.walkable_path


def test_tts_engine_synthesize():
    tts = TTSEngine()
    file_path = tts.synthesize_to_file("IntelliCane system initialized.")
    if tts.has_say:
        assert file_path is not None
