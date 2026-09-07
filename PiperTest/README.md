# PiperTest — Piper 한국어 TTS 로컬 최소 테스트

측정일 2026-09-07 / Windows 11 Pro 26200 / Ryzen 계열 24C32T / RAM 128GB / RTX 4060 Ti (미사용)

## 구성

| 항목 | 값 |
|---|---|
| Piper | piper-tts **1.8.0** (OHF-Voice/piper1-gpl, 2026-09-04 릴리스) |
| 실행 방식 | pip wheel (`piper_tts-1.8.0-cp39-abi3-win_amd64.whl`) + 전용 venv |
| Python | 3.11.9 (`C:\Users\user\AppData\Local\Programs\Python\Python311`) |
| CLI | `.venv\Scripts\piper.exe` |
| Python API | `from piper import PiperVoice, SynthesisConfig` |
| 추론 | onnxruntime 1.29.0, **CPUExecutionProvider** (GPU 미사용) |
| Voice | ko_KR-kss-medium (22050 Hz, mono, 16-bit, 60.3 MB) |

시스템 Python 3.14는 건드리지 않았다. `PiperTest\.venv` 안에만 설치.

## 재현

```powershell
cd C:\Dev\40TTS\PiperTest
# 1) 한 문장
.\.venv\Scripts\piper.exe -m ko_KR-kss-medium --data-dir .\voices -i .\scripts\one.txt -f .\output\test.wav
# 2) 측정
.\.venv\Scripts\python.exe .\scripts\cold.py           # cold start
.\.venv\Scripts\python.exe .\scripts\bench.py          # warm / TTFA / RTF (ORT 기본 스레드)
.\.venv\Scripts\python.exe .\scripts\bench_limited.py  # warm / TTFA / RTF (intra_op=2, 권장)
.\.venv\Scripts\python.exe .\scripts\stream_gen.py     # 생성 전용 chunk 타이밍
.\.venv\Scripts\python.exe .\scripts\stream.py         # 첫 chunk 즉시 재생 (streaming)
.\.venv\Scripts\python.exe .\scripts\resource.py       # RSS / CPU / DLL
.\.venv\Scripts\python.exe .\scripts\threads.py        # intra_op 스레드별 비교 + GPU 확인
.\.venv\Scripts\python.exe .\scripts\edge.py           # 숫자/영어/ㅋㅋㅋ/이모지 처리
```

결과 JSON은 `output\*.json`.

## 핵심 수치

- cold start (프로세스 신규 실행 → 첫 WAV 완료): **1155 ms**
- model load (in-process): **925 ms**
- warm synthesis: **42 ms** (ORT 기본) / **55–67 ms** (intra_op=2)
- TTFA = warm synthesis (문장 단위로 chunk가 한 번에 나온다)
- RTF: **0.026** (기본) / **0.036** (intra_op=2)
- RSS: 136 MB (로드 직후) → 213–223 MB (반복 합성 후)
- CPU: ORT 기본은 **24코어 / 시스템 75%** 를 순간 점유 → 게임과 동시 실행 시 **intra_op=2 필수** (2코어 / 6%)

## 게임 통합 시 주의

1. `PiperVoice.load()`는 프로세스당 1회. 문장마다 CLI를 새로 띄우면 매번 +1.1초.
2. `onnxruntime.SessionOptions(intra_op_num_threads=2, inter_op_num_threads=1)`로
   세션을 만들어야 한다. Piper 기본값은 전 코어를 잡는다.
3. streaming은 `PiperVoice.synthesize()`가 **문장 단위 generator**. 한 문장은 통째로
   나오므로 문장 내 TTFA 단축은 불가. 긴 대사는 문장을 쪼개 넣으면 첫 문장 43 ms에
   재생 시작 가능.
4. C++에서는 `libpiper` (C API, piper1-gpl 저장소) 또는 `piper.http_server`
   (`pip install piper-tts[http]`, flask 필요) 를 쓴다. 이번 테스트에서는 미검증.
