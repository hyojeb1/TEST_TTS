# -*- coding: utf-8 -*-
"""리소스 측정: RSS / CPU / 스레드수 / GPU 사용여부.

onnxruntime 기본값(전 코어 사용)과 intra_op=2 로 제한한 경우를 비교한다.
게임과 동시 실행 시 CPU 점유를 억제할 수 있는지 확인 목적.
"""
import json, os, statistics, time
from pathlib import Path
import onnxruntime as ort
import psutil
from piper import PiperVoice, SynthesisConfig

ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT / "voices" / "ko_KR-kss-medium.onnx"
SENTS = ["도네이션 감사합니다.", "아니 저걸 맞네.", "ㅋㅋㅋ 보스 패턴 뭐냐.",
         "형 거기 아니야.", "와 이걸 진짜 깨네."]
proc = psutil.Process()
syn = SynthesisConfig()

def bench(voice, label, seconds=6.0):
    list(voice.synthesize("초기화.", syn_config=syn))
    proc.cpu_percent(None)
    t_start = time.perf_counter()
    cpu0 = sum(proc.cpu_times()[:2])
    n = 0; synth_ms = []; audio_s = 0.0; peak = 0.0
    while time.perf_counter() - t_start < seconds:
        text = SENTS[n % len(SENTS)]
        t0 = time.perf_counter()
        chunks = list(voice.synthesize(text, syn_config=syn))
        synth_ms.append((time.perf_counter() - t0) * 1000)
        b = sum(len(c.audio_int16_bytes) for c in chunks)
        audio_s += b / 2 / chunks[0].sample_rate
        peak = max(peak, proc.memory_info().rss / 1024 / 1024)
        n += 1
    wall = time.perf_counter() - t_start
    cpu_used = sum(proc.cpu_times()[:2]) - cpu0
    return {
        "label": label,
        "iterations": n,
        "wall_s": round(wall, 2),
        "synth_ms_median": round(statistics.median(synth_ms), 1),
        "audio_generated_s": round(audio_s, 2),
        "rtf": round(sum(synth_ms) / 1000 / audio_s, 4),
        "rss_peak_mb": round(peak, 1),
        "num_threads": proc.num_threads(),
        # 100% == 논리코어 1개 100%
        "cpu_percent_of_one_core": round(cpu_used / wall * 100, 1),
        "cpu_percent_of_system": round(cpu_used / wall * 100 / psutil.cpu_count(), 2),
    }

out = {"logical_cores": psutil.cpu_count(),
       "physical_cores": psutil.cpu_count(logical=False),
       "total_ram_gb": round(psutil.virtual_memory().total / 1024**3, 1),
       "ort_version": ort.__version__,
       "ort_available_providers": ort.get_available_providers()}

rss_base = proc.memory_info().rss / 1024 / 1024
v = PiperVoice.load(MODEL, use_cuda=False)
out["rss_after_load_mb"] = round(proc.memory_info().rss / 1024 / 1024, 1)
out["rss_before_load_mb"] = round(rss_base, 1)
out["default_threads"] = bench(v, "onnxruntime default threads")

# intra_op 제한 세션으로 교체
so = ort.SessionOptions()
so.intra_op_num_threads = 2
so.inter_op_num_threads = 1
so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
v.session = ort.InferenceSession(str(MODEL), sess_options=so, providers=["CPUExecutionProvider"])
out["limited_threads_2"] = bench(v, "intra_op=2, inter_op=1")

out["rss_final_mb"] = round(proc.memory_info().rss / 1024 / 1024, 1)
out["model_file_mb"] = round(MODEL.stat().st_size / 1024 / 1024, 1)
out["loaded_dlls"] = sorted({Path(m.path).name for m in proc.memory_maps()
                             if m.path.lower().endswith(".dll")
                             and ("onnx" in m.path.lower() or "piper" in m.path.lower()
                                  or "espeak" in m.path.lower() or "vcruntime" in m.path.lower()
                                  or "msvcp" in m.path.lower())})
print(json.dumps(out, ensure_ascii=False, indent=2))
(ROOT/"output"/"resource.json").write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding="utf-8")
