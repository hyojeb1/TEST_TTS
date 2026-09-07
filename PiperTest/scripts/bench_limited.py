# -*- coding: utf-8 -*-
"""권장 배포 설정(intra_op=2)에서의 문장별 warm synthesis / TTFA / RTF."""
import json, statistics, time
from pathlib import Path
import onnxruntime as ort
from piper import PiperVoice, SynthesisConfig
ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT/"voices"/"ko_KR-kss-medium.onnx"
SENTS = [("01","도네이션 감사합니다."),("02","아니 저걸 맞네."),("03","ㅋㅋㅋ 보스 패턴 뭐냐."),
         ("04","형 거기 아니야."),("05","와 이걸 진짜 깨네.")]
syn = SynthesisConfig()
v = PiperVoice.load(MODEL, use_cuda=False)
so = ort.SessionOptions(); so.intra_op_num_threads = 2; so.inter_op_num_threads = 1
so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
t0=time.perf_counter()
v.session = ort.InferenceSession(str(MODEL), sess_options=so, providers=["CPUExecutionProvider"])
load_ms=(time.perf_counter()-t0)*1000
list(v.synthesize("초기화.", syn_config=syn))
rows=[]
for tag,text in SENTS:
    ms=[];ttfa=[];dur=[]
    for _ in range(7):
        t=time.perf_counter(); first=None; n=0; rate=None
        for ch in v.synthesize(text, syn_config=syn):
            if first is None: first=(time.perf_counter()-t)*1000
            n+=len(ch.audio_int16_bytes)//2; rate=ch.sample_rate
        ms.append((time.perf_counter()-t)*1000); ttfa.append(first); dur.append(n/rate)
    med=statistics.median(ms); d=statistics.median(dur)
    rows.append({"tag":tag,"text":text,"synth_ms_median":round(med,1),"synth_ms_min":round(min(ms),1),
                 "ttfa_ms_median":round(statistics.median(ttfa),1),"audio_s":round(d,3),
                 "rtf":round(med/1000/d,4)})
    print(rows[-1])
res={"config":"intra_op=2 / inter_op=1 / CPUExecutionProvider","session_create_ms":round(load_ms,1),
     "sentences":rows,
     "overall_rtf":round(sum(r["synth_ms_median"] for r in rows)/1000/sum(r["audio_s"] for r in rows),4)}
(ROOT/"output"/"bench_limited.json").write_text(json.dumps(res,ensure_ascii=False,indent=2),encoding="utf-8")
print("overall_rtf",res["overall_rtf"])
