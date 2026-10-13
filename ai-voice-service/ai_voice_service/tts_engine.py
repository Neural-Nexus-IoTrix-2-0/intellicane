import os
import shutil
import subprocess
import tempfile
from typing import Optional


class TTSEngine:
    """Text-To-Speech engine supporting local OS speech and neural cloud synthesis."""

    def __init__(self, output_dir: Optional[str] = None):
        self.output_dir = output_dir or tempfile.gettempdir()
        self.has_say = shutil.which("say") is not None

    def synthesize_to_file(self, text: str, filename: Optional[str] = None) -> Optional[str]:
        """Synthesizes text to an audio file and returns the file path."""
        if not filename:
            filename = f"speech_{abs(hash(text)) % 1000000}.aiff" if self.has_say else f"speech_{abs(hash(text)) % 1000000}.mp3"

        out_path = os.path.join(self.output_dir, filename)

        # 1. On macOS, use built-in 'say' tool (super fast, offline, natural voice)
        if self.has_say:
            try:
                subprocess.run(["say", "-v", "Samantha", "-o", out_path, text], check=True, timeout=5)
                return out_path
            except Exception:
                pass

        # 2. Try gTTS if installed
        try:
            from gtts import gTTS
            tts = gTTS(text=text, lang="en", slow=False)
            tts.save(out_path)
            return out_path
        except ImportError:
            pass
        except Exception:
            pass

        return None

    def speak_now(self, text: str):
        """Immediately speaks the text aloud through the host machine speakers (macOS / Linux)."""
        if self.has_say:
            try:
                subprocess.Popen(["say", "-v", "Samantha", text])
                return
            except Exception:
                pass

        # Fallback to synthesizing and playing via afplay or aplay
        path = self.synthesize_to_file(text)
        if path and os.path.exists(path):
            if shutil.which("afplay"):
                subprocess.Popen(["afplay", path])
            elif shutil.which("aplay"):
                subprocess.Popen(["aplay", path])
