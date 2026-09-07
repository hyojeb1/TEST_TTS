# -*- coding: utf-8 -*-
"""Streaming / TTFA 테스트.

PiperVoice.synthesize()는 문장 단위 generator이므로,
여러 문장을 한 번에 넣으면 첫 문장이 끝나는 즉시 재생을 시작할 수 있다.
여기서는 첫 chunk 도착 시각(TTFA)과 각 chunk 도착 시각을 기록하고,
실제로 첫 chunk가 오는 즉시 스피커로 재생을 시작한다.
"""
import sys, time, json
from pathlib import Path
import numpy as np
import sounddevice as sd
from piper import PiperVoice, SynthesisConfig

ROOT = Path(__file__).resolve().parent.parent
LONG = ("도네이션 감사합니다. 아니 저걸 맞네. ㅋㅋㅋ 보스 패턴 뭐냐. "
        "형 거기 아니야. 와 이걸 진짜 깨네.")

voice = PiperVoice.load(ROOT / "voices" / "ko_KR-kss-medium.onnx")
syn = SynthesisConfig()
list(voice.synthesize("초기화.", syn_config=syn))  # warm

stream = None
log = []
t0 = time.perf_counter()
audio_total = 0.0
for i, ch in enumerate(voice.synthesize(LONG, syn_config=syn)):
    t = (time.perf_counter() - t0) * 1000
    if stream is None:
        stream = sd.OutputStream(samplerate=ch.sample_rate, channels=1, dtype="int16")
        stream.start()
        ttfa = t
        t_play_start = (time.perf_counter() - t0) * 1000
    arr = np.frombuffer(ch.audio_int16_bytes, dtype=np.int16)
    stream.write(arr)          # 첫 chunk 도착 즉시 재생 시작
    dur = len(arr) / ch.sample_rate
    audio_total += dur
    log.append({"chunk": i, "ready_at_ms": round(t, 1), "audio_s": round(dur, 3),
                "phonemes": "".join(ch.phonemes)})
gen_done = (time.perf_counter() - t0) * 1000
stream.stop(); stream.close()
wall_done = (time.perf_counter() - t0) * 1000

res = {
    "text": LONG,
    "ttfa_ms": round(ttfa, 1),
    "playback_started_at_ms": round(t_play_start, 1),
    "generation_done_ms": round(gen_done, 1),
    "wall_incl_playback_ms": round(wall_done, 1),
    "total_audio_s": round(audio_total, 3),
    "rtf_streaming": round(gen_done / 1000.0 / audio_total, 4),
    "chunks": log,
}
print(json.dumps(res, ensure_ascii=False, indent=2))
(ROOT / "output" / "stream.json").write_text(json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
