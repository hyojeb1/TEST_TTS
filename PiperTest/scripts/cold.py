# -*- coding: utf-8 -*-
"""Cold start 측정: 프로세스 신규 실행 -> 모델 로딩 -> 첫 문장 WAV 완료."""
import json, statistics, subprocess, sys, time
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / ".venv" / "Scripts" / "piper.exe"
TXT = ROOT / "scripts" / "one.txt"
TXT.write_text("도네이션 감사합니다.", encoding="utf-8")  # BOM 없음
N = 5
times = []
for i in range(N):
    out = ROOT / "output" / f"cold_{i}.wav"
    t0 = time.perf_counter()
    r = subprocess.run([str(EXE), "-m", "ko_KR-kss-medium", "--data-dir", str(ROOT/"voices"),
                        "-i", str(TXT), "-f", str(out)],
                       capture_output=True)
    dt = (time.perf_counter() - t0) * 1000
    assert r.returncode == 0, r.stderr.decode("utf-8", "replace")
    assert out.stat().st_size > 1000
    times.append(dt)
print(json.dumps({"cold_start_ms": [round(t,1) for t in times],
                  "cold_start_ms_median": round(statistics.median(times),1),
                  "cold_start_ms_min": round(min(times),1)}, indent=2))
