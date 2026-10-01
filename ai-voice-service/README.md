# AI Voice Service (Phase 3)

## Purpose

The AI Voice Service is an off-device cloud or mobile companion service designed to provide natural language scene understanding and audio feedback to the user.

## Architecture

The ESP32 microcontroller cannot locally execute modern Vision-Language Models (VLMs) or full neural Text-to-Speech (TTS) models. Therefore:
1. **User Audio Input / Trigger**: User presses a query button or speaks a prompt (via smartphone companion app or Bluetooth headset mic).
2. **Snapshot Uplink**: The ESP32-CAM pushes the latest captured frame.
3. **Multimodal Inference**: Cloud/local server invokes a Vision-Language Model (e.g., Gemini 1.5 Flash / GPT-4o-mini) with context-specific prompts ("Describe immediate hazards, pedestrian signals, or text in view concisely").
4. **Speech Synthesis**: Response is converted to natural speech via TTS (e.g., Google Cloud TTS, Edge TTS, or ElevenLabs) and streamed to the user's earpiece.

## Planned Stack

- **Runtime**: Python (FastAPI) or Node.js.
- **Vision-Language Model**: Google Gemini API / OpenAI API.
- **Speech Processing**: Whisper API (STT) + Edge-TTS / gTTS.
