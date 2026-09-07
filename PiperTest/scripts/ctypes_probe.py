# 진단 전용: piper.dll 을 ctypes 로 직접 호출해 C++ 프로그램과 동일한 경로/인자로 시험한다.
import ctypes, os, sys
from ctypes import c_char_p, c_int, c_float, c_size_t, c_void_p, POINTER, Structure, byref

LIB = r"C:\Dev\40TTS\PiperTest\third_party\piper1-gpl\libpiper\install\lib"
os.add_dll_directory(LIB)
dll = ctypes.CDLL(os.path.join(LIB, "piper.dll"))

class Chunk(Structure):
    _fields_ = [("samples", POINTER(c_float)), ("num_samples", c_size_t),
                ("sample_rate", c_int), ("is_last", ctypes.c_bool),
                ("phonemes", c_void_p), ("num_phonemes", c_size_t),
                ("phoneme_ids", c_void_p), ("num_phoneme_ids", c_size_t),
                ("alignments", c_void_p), ("num_alignments", c_size_t)]

class Opts(Structure):
    _fields_ = [("speaker_id", c_int), ("length_scale", c_float),
                ("noise_scale", c_float), ("noise_w_scale", c_float)]

dll.piper_version.restype = c_char_p
dll.piper_create.restype = c_void_p
dll.piper_create.argtypes = [c_char_p, c_char_p, c_char_p]
dll.piper_default_synthesize_options.restype = Opts
dll.piper_default_synthesize_options.argtypes = [c_void_p]
dll.piper_synthesize_start.restype = c_int
dll.piper_synthesize_start.argtypes = [c_void_p, c_char_p, POINTER(Opts)]
dll.piper_synthesize_next.restype = c_int
dll.piper_synthesize_next.argtypes = [c_void_p, POINTER(Chunk)]
dll.piper_free.argtypes = [c_void_p]

print("version", dll.piper_version().decode())
model = rb"C:\Dev\40TTS\PiperTest\voices\ko_KR-kss-medium.onnx"
cfg   = rb"C:\Dev\40TTS\PiperTest\voices\ko_KR-kss-medium.onnx.json"
espk  = rb"C:\Dev\40TTS\PiperTest\third_party\piper1-gpl\libpiper\install\share\espeak-ng-data"

synth = dll.piper_create(model, cfg, espk)
print("piper_create ->", synth)
if not synth:
    sys.exit("create failed")

o = dll.piper_default_synthesize_options(synth)
print("default options: speaker=%d length=%.3f noise=%.3f noise_w=%.3f"
      % (o.speaker_id, o.length_scale, o.noise_scale, o.noise_w_scale))

for label, text in [("ascii", b"Hello world"),
                    ("korean", "도네이션 감사합니다".encode("utf-8"))]:
    rc = dll.piper_synthesize_start(synth, text, byref(o))
    print(f"[{label}] start rc={rc}")
    total, n = 0, 0
    ch = Chunk()
    while True:
        rc = dll.piper_synthesize_next(synth, byref(ch))
        if rc == 1:
            break
        if rc != 0:
            print("  next rc=", rc); break
        n += 1; total += ch.num_samples
    print(f"[{label}] chunks={n} samples={total} rate={ch.sample_rate}")
dll.piper_free(synth)
