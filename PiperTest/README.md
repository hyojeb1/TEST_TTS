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

---

# C++ 네이티브 경로 (libpiper) — Python 없음

`ConsoleApplication1` 이 `piper.dll` 을 직접 링크해 합성한다. Python / piper CLI 미사용.

## 빌드

```
third_party\piper1-gpl\libpiper\build_win_x64.bat
```

MSVC 2022 (v143) + Ninja, Release x64. espeak-ng 는 소스에서 빌드되고
onnxruntime-win-x64-1.22.0 은 자동 다운로드된다. 첫 빌드 약 5분.

빌드 결과 `third_party\piper1-gpl\libpiper\install\`:

| 파일 | 크기 | 용도 |
|---|---:|---|
| `include\piper.h` | 8.8 KB | C API 헤더 |
| `lib\piper.lib` | 36 KB | import 라이브러리 |
| `lib\piper.dll` | 875 KB | libpiper (espeak-ng 정적 링크 포함) |
| `lib\onnxruntime.dll` | 12.4 MB | onnxruntime 1.22.0 |
| `lib\onnxruntime_providers_shared.dll` | 22 KB | |
| `share\espeak-ng-data\` | 19 MB | 음소 사전 (ko_dict 포함) |

## Windows 에서 걸린 함정 3개

1. **MSVC 소스 문자셋** — `chinese_phonemizer.cpp` / `piper.cpp` 는 BOM 없는 UTF-8.
   한국어 로케일에서는 CP949 로 읽혀 `error C3688` 이 난다. `/utf-8` 필요.
2. **`CMAKE_CXX_FLAGS` 는 덮어쓴다** — `/utf-8` 만 넘기면 CMake 기본값
   `/DWIN32 /D_WINDOWS /EHsc` 가 사라진다. `WIN32` 가 없으면 `piper.cpp` 의
   wchar_t 경로 변환 분기가 죽어 `Ort::Session` 이 `error C2440` 으로 실패한다.
   기본값을 함께 다시 지정해야 한다.
3. **`C:\Windows\System32\onnxruntime.dll` 은 1.17** (Windows ML 기본 탑재).
   piper.dll 은 ORT API 22 를 요구하므로, exe 와 같은 폴더에 1.22.0 을
   두지 않으면 System32 것이 로드되어
   `The requested API version [22] is not available` 로 죽는다.

## libpiper API 사용 시 반드시 알아야 할 것

`piper_synthesize_next` 는 **마지막 chunk 를 `PIPER_DONE` 과 함께** 반환한다.
`PIPER_DONE` 은 "데이터 없음"이 아니라 "이게 마지막"이라는 뜻이다.
libpiper README 의 예제

```c++
while (piper_synthesize_next(synth, &chunk) != PIPER_DONE) { ... }   // 틀림
```

는 마지막 chunk 를 버리므로 **문장이 하나뿐인 입력에서는 오디오가 0 바이트**다.
상류 `piper_exe` 와 같이 반환값이 아니라 chunk 필드로 순회해야 한다.

```c++
for (;;) {
    int rc = piper_synthesize_next(synth, &chunk);
    if (rc != PIPER_OK && rc != PIPER_DONE) { /* error */ }
    if (chunk.samples && chunk.num_samples > 0) { /* consume */ }
    if (rc == PIPER_DONE || chunk.is_last) break;
}
```

## C++ 성능 (동일 머신, "도네이션 감사합니다")

| | C++ libpiper | Python piper-tts |
|---|---:|---:|
| model load | 465–501 ms | 925 ms |
| TTFA / synthesis | **131 ms** | 43 ms |
| audio duration | 1.858 s | 1.834 s |
| RTF | **0.071** | 0.026 |

C++ 쪽이 느린 이유는 명확하다. `libpiper/src/piper.cpp` 가
`SetIntraOpNumThreads(1)` / `SetInterOpNumThreads(1)` 로 **하드코딩**되어 있다.
Python 판의 intra_op=1 측정값(102 ms, RTF 0.059)과 같은 수준이다.
스레드 수를 바꾸려면 libpiper 를 수정해야 하며, 현재 C API 로는 노출되지 않는다.
게임 동시 실행 관점에서는 1 코어만 쓰는 이 기본값이 오히려 유리하다.

## 출력 형식 차이

libpiper 는 모델 raw float32 를 그대로 준다 (peak 0.49). Python API 의
`normalize_audio=True` 에 해당하는 정규화가 없으므로 C++ 출력이 더 작다.
필요하면 호출측에서 정규화한다.

## 진단 스크립트

디버깅 과정에서 만든 것으로, 문제 격리에 유용하다.

* `scripts\ctypes_probe.py` — piper.dll 의 C API 를 ctypes 로 직접 호출
* `scripts\espeak_probe.py` — piper.dll 이 재수출하는 espeak 심볼을 직접 호출
