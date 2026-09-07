# -*- coding: utf-8 -*-
"""숫자/영어/이모지/문장부호 등 게임 채팅 표기 처리 확인."""
import json, wave
from pathlib import Path
from piper import PiperVoice, SynthesisConfig
ROOT = Path(__file__).resolve().parent.parent
CASES = [
    ("e01", "ㅋㅋㅋ"),
    ("e02", "ㅋㅋㅋㅋㅋㅋㅋㅋ"),
    ("e03", "ㅎㅎ ㅠㅠ ㅗㅜㅑ"),
    ("e04", "5000원 도네이션 감사합니다!"),
    ("e05", "HP 30% 남았어요"),
    ("e06", "GG ez win"),
    ("e07", "보스 체력 1,250,000 남음"),
    ("e08", "와... 이걸 깨네?!"),
    ("e09", "형?? 거기 아니야ㅠㅠ"),
    ("e10", "닉네임: Player_01 님 감사합니다"),
    ("e11", "😂👍 개웃기다"),
]
v = PiperVoice.load(ROOT/"voices"/"ko_KR-kss-medium.onnx")
syn = SynthesisConfig()
out = []
for tag, text in CASES:
    chunks = list(v.synthesize(text, syn_config=syn))
    if chunks:
        b = b"".join(c.audio_int16_bytes for c in chunks)
        with wave.open(str(ROOT/"output"/f"{tag}.wav"), "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(chunks[0].sample_rate)
            w.writeframes(b)
        dur = round(len(b)/2/chunks[0].sample_rate, 3)
    else:
        dur = 0.0
    ph = "".join("".join(p) for p in v.phonemize(text))
    out.append({"tag": tag, "text": text, "audio_s": dur, "phonemes": ph})
    print(f"{tag} {dur:>6.3f}s  {text!r}\n      -> {ph}")
(ROOT/"output"/"edge.json").write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding="utf-8")
