# -*- coding: utf-8 -*-
"""Piper ko_KR-kss-medium 로컬 성능 측정 (CPU inference).

측정 항목:
  - model load (in-process)
  - warm synthesis (문장별, N회 반복)
  - TTFA (첫 AudioChunk 도착 시간)
  - audio duration / RTF
  - RSS / CPU time
"""
import io
import json
import os
import statistics
import sys
import time
import wave
from pathlib import Path

import psutil

ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT / "voices" / "ko_KR-kss-medium.onnx"
OUT = ROOT / "output"
OUT.mkdir(exist_ok=True)

SENTENCES = [
    ("01", "도네이션 감사합니다."),
    ("02", "아니 저걸 맞네."),
    ("03", "ㅋㅋㅋ 보스 패턴 뭐냐."),
    ("04", "형 거기 아니야."),
    ("05", "와 이걸 진짜 깨네."),
]
RUNS = int(os.environ.get("RUNS", "7"))

proc = psutil.Process()
def rss_mb():
    return proc.memory_info().rss / 1024 / 1024

def cpu_s():
    c = proc.cpu_times()
    return c.user + c.system

t_import0 = time.perf_counter()
from piper import PiperVoice, SynthesisConfig  # noqa: E402
t_import = (time.perf_counter() - t_import0) * 1000
rss_after_import = rss_mb()

t0 = time.perf_counter()
voice = PiperVoice.load(MODEL, use_cuda=False)
t_load = (time.perf_counter() - t0) * 1000
rss_after_load = rss_mb()

syn = SynthesisConfig()

# onnxruntime 첫 실행 그래프 초기화 비용을 분리하기 위한 in-process 첫 합성
t0 = time.perf_counter()
_ = list(voice.synthesize("초기화.", syn_config=syn))
t_first_inproc = (time.perf_counter() - t0) * 1000
rss_after_first = rss_mb()

results = []
for tag, text in SENTENCES:
    full_ms, ttfa_ms, dur_s, nchunks = [], [], [], []
    for i in range(RUNS):
        t0 = time.perf_counter()
        first = None
        frames = 0
        rate = None
        width = None
        chunks = []
        for ch in voice.synthesize(text, syn_config=syn):
            if first is None:
                first = (time.perf_counter() - t0) * 1000
            rate = ch.sample_rate
            width = ch.sample_width
            b = ch.audio_int16_bytes
            frames += len(b) // (width * ch.sample_channels)
            chunks.append(b)
        total = (time.perf_counter() - t0) * 1000
        full_ms.append(total)
        ttfa_ms.append(first)
        dur_s.append(frames / rate)
        nchunks.append(len(chunks))
        if i == 0:
            p = OUT / f"{tag}.wav"
            with wave.open(str(p), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(width)
                w.setframerate(rate)
                w.writeframes(b"".join(chunks))
    med = statistics.median(full_ms)
    d = statistics.median(dur_s)
    results.append({
        "tag": tag, "text": text,
        "chunks": nchunks[0],
        "synth_ms_median": round(med, 1),
        "synth_ms_min": round(min(full_ms), 1),
        "synth_ms_max": round(max(full_ms), 1),
        "ttfa_ms_median": round(statistics.median(ttfa_ms), 1),
        "audio_s": round(d, 3),
        "rtf": round(med / 1000.0 / d, 4),
        "phonemes": voice.phonemize(text),
    })

rss_peak = rss_mb()
summary = {
    "python": sys.version.split()[0],
    "import_ms": round(t_import, 1),
    "model_load_ms": round(t_load, 1),
    "first_inproc_synth_ms": round(t_first_inproc, 1),
    "rss_after_import_mb": round(rss_after_import, 1),
    "rss_after_load_mb": round(rss_after_load, 1),
    "rss_after_first_synth_mb": round(rss_after_first, 1),
    "rss_peak_mb": round(rss_peak, 1),
    "cpu_time_s": round(cpu_s(), 2),
    "runs_per_sentence": RUNS,
    "sample_rate": results[0] and 22050,
    "sentences": results,
}
tot_synth = sum(r["synth_ms_median"] for r in results)
tot_audio = sum(r["audio_s"] for r in results)
summary["overall_rtf"] = round(tot_synth / 1000.0 / tot_audio, 4)
summary["providers"] = None
try:
    summary["providers"] = voice.session.get_providers()
except Exception:
    pass

print(json.dumps(summary, ensure_ascii=False, indent=2))
(ROOT / "output" / "bench.json").write_text(
    json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
