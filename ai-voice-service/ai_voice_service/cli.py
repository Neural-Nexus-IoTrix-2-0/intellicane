import argparse
import sys
import os
import requests
import uvicorn

from .analyzer import SceneAnalyzer
from .tts_engine import TTSEngine


def main():
    parser = argparse.ArgumentParser(description="IntelliCane AI Scene Understanding CLI")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # Command: cane
    cane_parser = subparsers.add_parser("cane", help="Trigger and analyze snapshot from ESP32-CAM")
    cane_parser.add_argument("--url", default="http://192.168.4.1/capture", help="ESP32-CAM capture URL")
    cane_parser.add_argument("--key", default=None, help="Gemini API Key")
    cane_parser.add_argument("--no-speak", action="store_true", help="Disable audio speech readout")

    # Command: image
    img_parser = subparsers.add_parser("image", help="Analyze a local image file")
    img_parser.add_argument("path", help="Path to JPEG image file")
    img_parser.add_argument("--key", default=None, help="Gemini API Key")
    img_parser.add_argument("--no-speak", action="store_true", help="Disable audio speech readout")

    # Command: serve
    serve_parser = subparsers.add_parser("serve", help="Start FastAPI service")
    serve_parser.add_argument("--host", default="0.0.0.0", help="Host address")
    serve_parser.add_argument("--port", type=int, default=8000, help="Port number")

    # Command: demo
    demo_parser = subparsers.add_parser("demo", help="Run simulated blind assistance demo")

    args = parser.parse_args()

    if not args.command or args.command == "demo":
        run_demo()
        return

    analyzer = SceneAnalyzer()
    tts = TTSEngine()

    if args.command == "cane":
        print(f"[CLI] Fetching frame from ESP32-CAM: {args.url} ...")
        try:
            resp = requests.get(args.url, timeout=6)
            resp.raise_for_status()
            img_bytes = resp.content
        except Exception as e:
            print(f"[ERROR] Failed to fetch image: {e}")
            sys.exit(1)

        result = analyzer.analyze(img_bytes, api_key=args.key)
        print_result(result)
        if not args.no_speak:
            tts.speak_now(result.spoken_summary)

    elif args.command == "image":
        if not os.path.exists(args.path):
            print(f"[ERROR] File not found: {args.path}")
            sys.exit(1)

        with open(args.path, "rb") as f:
            img_bytes = f.read()

        result = analyzer.analyze(img_bytes, api_key=args.key)
        print_result(result)
        if not args.no_speak:
            tts.speak_now(result.spoken_summary)

    elif args.command == "serve":
        print(f"[CLI] Starting IntelliCane AI Voice Service on {args.host}:{args.port}")
        uvicorn.run("ai_voice_service.server:app", host=args.host, port=args.port, reload=False)


def run_demo():
    print("\n========================================================")
    print("   IntelliCane Blind Navigation AI Assistant Demo      ")
    print("========================================================")
    analyzer = SceneAnalyzer()
    tts = TTSEngine()

    # Create dummy 1x1 jpeg bytes for demo
    dummy_jpg = b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x01\x00`\x00`\x00\x00\xff\xdb\x00C\x00\x08\x06\x06\x07\x06\x05\x08\x07\x07\x07\t\t\x08\n\x0c\x14\r\x0c\x0b\x0b\x0c\x19\x12\x13\x0f\x14\x1d\x1a\x1f\x1e\x1d\x1a\x1c\x1c $.\' \",#\x1c\x1c(7),01444\x1f\'9=82<.342\xff\xc0\x00\x0b\x08\x00\x01\x00\x01\x01\x01\x11\x00\xff\xc4\x00\x1f\x00\x00\x01\x05\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\xff\xda\x00\x08\x01\x01\x00\x00?\x00\xbf\x00\xff\xd9'

    result = analyzer.analyze(dummy_jpg)
    print_result(result)
    print("\n[AUDIO] Synthesizing speech aloud...")
    tts.speak_now(result.spoken_summary)
    print("========================================================\n")


def print_result(result):
    print("\n" + "=" * 50)
    print("  SCENE UNDERSTANDING REPORT (Blind Navigation)")
    print("=" * 50)
    print(f"Spoken Guidance:  {result.spoken_summary}")
    print(f"Urgent Hazards:   {', '.join(result.urgent_hazards) if result.urgent_hazards else 'None'}")
    print(f"Walkable Path:    {result.walkable_path}")
    print(f"Text / Signs:     {result.text_and_signs}")
    print("\nSurrounding Objects:")
    for obj in result.detected_objects:
        print(f"  • {obj.name} at {obj.position} ({obj.distance}) - Hazard: {obj.hazard_level}")
    print(f"\nProcessing Time:  {result.latency_ms} ms")
    print("=" * 50 + "\n")


if __name__ == "__main__":
    main()
