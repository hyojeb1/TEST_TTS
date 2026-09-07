# -*- coding: utf-8 -*-
"""intra_op 스레드 수별 RTF / CPU 점유 비교 + GPU 사용 여부 확인."""
import json, statistics, subprocess, time
from pathlib import Path
import onnxruntime as ort, psutil
from piper import PiperVoice, SynthesisConfig
ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT/"voices"/"ko_KR-kss-medium.onnx"
SENTS = ["도네이션 감사합니다.", "아니 저걸 맞네.", "ㅋㅋㅋ 보스 패턴 뭐냐.",
         "형 거기 아니야.", "와 이걸 진짜 깨네."]
proc = psutil.Process(); syn = SynthesisConfig()
v = PiperVoice.load(MODEL, use_cuda=False)
rows = []
for nt in [1, 2, 4, 0]:  # 0 = onnxruntime 기본
    so = ort.SessionOptions()
    if nt:
        so.intra_op_num_threads = nt
        so.inter_op_num_threads = 1
        so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
    v.session = ort.InferenceSession(str(MODEL), sess_options=so, providers=["CPUExecutionProvider"])
    list(v.synthesize("초기화.", syn_config=syn))
    ms, audio = [], 0.0
    c0 = sum(proc.cpu_times()[:2]); t0 = time.perf_counter()
    for i in range(25):
        t = time.perf_counter()
        ch = list(v.synthesize(SENTS[i % 5], syn_config=syn))
        ms.append((time.perf_counter()-t)*1000)
        audio += sum(len(c.audio_int16_bytes) for c in ch)/2/ch[0].sample_rate
    wall = time.perf_counter()-t0; cpu = sum(proc.cpu_times()[:2])-c0
    rows.append({"intra_op": nt or "default(auto)",
                 "synth_ms_median": round(statistics.median(ms),1),
                 "rtf": round(sum(ms)/1000/audio, 4),
                 "cores_used": round(cpu/wall, 1),
                 "cpu_pct_of_32c": round(cpu/wall/32*100, 1)})
    print(rows[-1])
# GPU 확인
gpu = subprocess.run(["nvidia-smi","--query-compute-apps=pid,process_name,used_memory",
                      "--format=csv,noheader"], capture_output=True, text=True).stdout.strip()
res = {"rows": rows, "ort_providers": ort.get_available_providers(),
       "nvidia_smi_compute_apps_during_run": gpu or "(none)"}
print(json.dumps(res, ensure_ascii=False, indent=2))
(ROOT/"output"/"threads.json").write_text(json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
