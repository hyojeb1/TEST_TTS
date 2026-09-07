# -*- coding: utf-8 -*-
"""생성 전용 streaming 타이밍 (재생 blocking 배제).

stream.py는 sounddevice write()가 blocking이라 2번째 chunk 이후 시각이
'재생 대기' 시간에 오염된다. 여기서는 재생 없이 순수 생성 타이밍만 본다.
"""
import json, time
from pathlib import Path
from piper import PiperVoice, SynthesisConfig

ROOT = Path(__file__).resolve().parent.parent
LONG = ("도네이션 감사합니다. 아니 저걸 맞네. ㅋㅋㅋ 보스 패턴 뭐냐. "
        "형 거기 아니야. 와 이걸 진짜 깨네.")
voice = PiperVoice.load(ROOT / "voices" / "ko_KR-kss-medium.onnx")
syn = SynthesisConfig()
list(voice.synthesize("초기화.", syn_config=syn))

log, audio_total = [], 0.0
t0 = time.perf_counter()
for i, ch in enumerate(voice.synthesize(LONG, syn_config=syn)):
    t = (time.perf_counter() - t0) * 1000
    n = len(ch.audio_int16_bytes) // 2
    dur = n / ch.sample_rate
    audio_total += dur
    log.append({"chunk": i, "ready_at_ms": round(t, 1),
                "audio_s": round(dur, 3),
                "cumulative_audio_s": round(audio_total, 3)})
gen = (time.perf_counter() - t0) * 1000
res = {"ttfa_ms": log[0]["ready_at_ms"],
       "generation_done_ms": round(gen, 1),
       "total_audio_s": round(audio_total, 3),
       "rtf_generation_only": round(gen / 1000.0 / audio_total, 4),
       "chunks": log}
print(json.dumps(res, ensure_ascii=False, indent=2))
(ROOT/"output"/"stream_gen.json").write_text(json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
