# 진단 전용: piper.dll 이 재수출하는 espeak 심볼을 직접 호출해
# 음소 변환이 되는지, 어느 단계에서 실패하는지 확인한다.
import ctypes, os
from ctypes import c_char_p, c_int, c_void_p, POINTER, byref

LIB = r"C:\Dev\40TTS\PiperTest\third_party\piper1-gpl\libpiper\install\lib"
DATA = r"C:\Dev\40TTS\PiperTest\third_party\piper1-gpl\libpiper\install\share\espeak-ng-data"
os.add_dll_directory(LIB)
d = ctypes.CDLL(os.path.join(LIB, "piper.dll"))

AUDIO_OUTPUT_SYNCHRONOUS = 2
espeakCHARS_AUTO = 0
espeakPHONEMES_IPA = 0x02

d.espeak_Initialize.restype = c_int
d.espeak_Initialize.argtypes = [c_int, c_int, c_char_p, c_int]
d.espeak_SetVoiceByName.restype = c_int
d.espeak_SetVoiceByName.argtypes = [c_char_p]
d.espeak_TextToPhonemesWithTerminator.restype = c_char_p
d.espeak_TextToPhonemesWithTerminator.argtypes = [POINTER(c_void_p), c_int, c_int, POINTER(c_int)]
d.espeak_Info.restype = c_char_p
d.espeak_Info.argtypes = [POINTER(c_char_p)]

rc = d.espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, DATA.encode(), 0)
print("espeak_Initialize ->", rc, "(음수면 실패, 성공시 sample rate)")

path_out = c_char_p()
print("espeak_Info       ->", d.espeak_Info(byref(path_out)))
print("espeak data path  ->", path_out.value)

print("SetVoiceByName ko ->", d.espeak_SetVoiceByName(b"ko"), "(0 == EE_OK)")

for label, text in [("ascii", b"Hello world."),
                    ("korean", "도네이션 감사합니다.".encode("utf-8"))]:
    buf = ctypes.create_string_buffer(text)
    ptr = c_void_p(ctypes.addressof(buf))
    term = c_int(0)
    out = []
    guard = 0
    while ptr.value and guard < 20:
        ph = d.espeak_TextToPhonemesWithTerminator(byref(ptr), espeakCHARS_AUTO,
                                                   espeakPHONEMES_IPA, byref(term))
        out.append(("" if ph is None else ph.decode("utf-8", "replace"), term.value & 0xFFFFF))
        guard += 1
    print(f"[{label}] -> {out}")
