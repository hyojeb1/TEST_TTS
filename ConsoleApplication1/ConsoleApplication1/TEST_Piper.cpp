// TEST_Piper.cpp
//
// libpiper (piper1-gpl) C API 를 직접 호출해 한국어 문장을 합성한다.
// Python / piper CLI 를 전혀 쓰지 않고 piper.dll + onnxruntime.dll 만 사용한다.
//
// 출력:
//   out\donation.raw  32-bit float mono PCM (piper 가 그대로 내주는 형식)
//   out\donation.wav  16-bit PCM WAV (Windows 에서 바로 재생 가능)
//
// 이 파일은 UTF-8 BOM 으로 저장해야 한다. MSVC 는 BOM 을 보고 소스를 UTF-8 로
// 해석하며, BOM 이 없으면 한국어 로케일에서 CP949 로 잘못 읽어 리터럴이 깨진다.
// 문자열은 wide 리터럴로 두고 WideCharToMultiByte(CP_UTF8) 로 변환한다.
// piper_synthesize_start 는 UTF-8 바이트를 요구하므로, 실행 문자셋(CP949)에
// 의존하는 좁은 리터럴을 그대로 넘기면 안 된다.

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <piper.h>

namespace {

// 기본 경로. argv 로 덮어쓸 수 있다.
constexpr const char* kDefaultModel =
    "C:\\Dev\\40TTS\\PiperTest\\voices\\ko_KR-kss-medium.onnx";
constexpr const char* kDefaultConfig =
    "C:\\Dev\\40TTS\\PiperTest\\voices\\ko_KR-kss-medium.onnx.json";
constexpr const char* kDefaultEspeakData =
    "C:\\Dev\\40TTS\\PiperTest\\third_party\\piper1-gpl\\libpiper\\install\\share\\espeak-ng-data";
constexpr const char* kOutDir = "out";

using Clock = std::chrono::steady_clock;

double MsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

std::string ToUtf8(const wchar_t* wide) {
    if (wide == nullptr) {
        return std::string();
    }
    const int needed =
        ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(needed) - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], needed, nullptr, nullptr);
    return out;
}

void PutU32(std::vector<unsigned char>& buf, uint32_t v) {
    buf.push_back(static_cast<unsigned char>(v & 0xFFu));
    buf.push_back(static_cast<unsigned char>((v >> 8) & 0xFFu));
    buf.push_back(static_cast<unsigned char>((v >> 16) & 0xFFu));
    buf.push_back(static_cast<unsigned char>((v >> 24) & 0xFFu));
}

void PutU16(std::vector<unsigned char>& buf, uint16_t v) {
    buf.push_back(static_cast<unsigned char>(v & 0xFFu));
    buf.push_back(static_cast<unsigned char>((v >> 8) & 0xFFu));
}

void PutTag(std::vector<unsigned char>& buf, const char* tag) {
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<unsigned char>(tag[i]));
    }
}

// 44-byte canonical RIFF/WAVE header, 16-bit PCM mono.
std::vector<unsigned char> MakeWavHeader(uint32_t num_samples, uint32_t sample_rate) {
    const uint16_t channels = 1;
    const uint16_t bits = 16;
    const uint32_t data_bytes = num_samples * (bits / 8) * channels;

    std::vector<unsigned char> h;
    h.reserve(44);
    PutTag(h, "RIFF");
    PutU32(h, 36 + data_bytes);
    PutTag(h, "WAVE");
    PutTag(h, "fmt ");
    PutU32(h, 16);                                   // fmt chunk size
    PutU16(h, 1);                                    // PCM
    PutU16(h, channels);
    PutU32(h, sample_rate);
    PutU32(h, sample_rate * channels * (bits / 8));  // byte rate
    PutU16(h, channels * (bits / 8));                // block align
    PutU16(h, bits);
    PutTag(h, "data");
    PutU32(h, data_bytes);
    return h;
}

bool WriteAll(const std::string& path, const void* data, size_t bytes) {
    FILE* f = nullptr;
    if (::fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr) {
        std::printf("  [ERROR] cannot open %s for writing\n", path.c_str());
        return false;
    }
    const size_t written = std::fwrite(data, 1, bytes, f);
    std::fclose(f);
    if (written != bytes) {
        std::printf("  [ERROR] short write to %s\n", path.c_str());
        return false;
    }
    return true;
}

int16_t FloatToPcm16(float sample) {
    float v = sample * 32767.0F;
    if (v > 32767.0F) {
        v = 32767.0F;
    }
    if (v < -32768.0F) {
        v = -32768.0F;
    }
    return static_cast<int16_t>(v);
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    ::SetConsoleOutputCP(CP_UTF8);

    const std::string text =
        (argc > 1) ? ToUtf8(argv[1]) : ToUtf8(L"도네이션 감사합니다");
    const std::string model_path = (argc > 2) ? ToUtf8(argv[2]) : kDefaultModel;
    const std::string config_path = (argc > 3) ? ToUtf8(argv[3]) : kDefaultConfig;
    const std::string espeak_data = (argc > 4) ? ToUtf8(argv[4]) : kDefaultEspeakData;

    std::printf("libpiper version : %s\n", piper_version());
    std::printf("model            : %s\n", model_path.c_str());
    std::printf("espeak-ng-data   : %s\n", espeak_data.c_str());
    std::printf("text             : %s\n\n", text.c_str());

    // ---- 모델 로딩 ----
    const Clock::time_point t_load = Clock::now();
    piper_synthesizer* synth =
        piper_create(model_path.c_str(), config_path.c_str(), espeak_data.c_str());
    const double load_ms = MsSince(t_load);

    if (synth == nullptr) {
        std::printf("[FAIL] piper_create returned NULL. "
                    "Check the model, config and espeak-ng-data paths.\n");
        return 1;
    }
    std::printf("model load       : %.1f ms\n", load_ms);

    // ---- 합성 ----
    piper_synthesize_options options = piper_default_synthesize_options(synth);

    const Clock::time_point t_synth = Clock::now();
    if (piper_synthesize_start(synth, text.c_str(), &options) != PIPER_OK) {
        std::printf("[FAIL] piper_synthesize_start failed\n");
        piper_free(synth);
        return 1;
    }

    std::vector<float> pcm;
    int sample_rate = 0;
    int num_chunks = 0;
    double ttfa_ms = -1.0;

    // piper_synthesize_next 는 마지막 chunk 를 PIPER_DONE 과 '함께' 반환한다.
    // 즉 PIPER_DONE 은 "데이터 없음"이 아니라 "이게 마지막"이라는 뜻이다.
    // libpiper README 의 while (next(...) != PIPER_DONE) 예제는 그 마지막
    // chunk 를 버리므로, 문장이 하나뿐인 입력에서는 오디오가 전혀 나오지 않는다.
    // 상류 piper_exe(src/main/utils/wavfile.cpp)와 같이 반환값이 아니라
    // chunk.num_samples / chunk.is_last 를 기준으로 순회해야 한다.
    piper_audio_chunk chunk = piper_audio_chunk();
    for (;;) {
        const int rc = piper_synthesize_next(synth, &chunk);
        if (rc != PIPER_OK && rc != PIPER_DONE) {
            std::printf("[FAIL] piper_synthesize_next returned %d\n", rc);
            piper_free(synth);
            return 1;
        }
        if (chunk.samples != nullptr && chunk.num_samples > 0) {
            if (ttfa_ms < 0.0) {
                ttfa_ms = MsSince(t_synth);
            }
            sample_rate = chunk.sample_rate;
            ++num_chunks;
            // chunk.samples 는 다음 호출에서 무효화되므로 반드시 복사한다.
            pcm.insert(pcm.end(), chunk.samples, chunk.samples + chunk.num_samples);
        }
        if (rc == PIPER_DONE || chunk.is_last) {
            break;
        }
    }
    const double synth_ms = MsSince(t_synth);
    piper_free(synth);

    if (pcm.empty() || sample_rate <= 0) {
        std::printf("[FAIL] no audio produced\n");
        return 1;
    }

    // ---- 저장 ----
    ::CreateDirectoryA(kOutDir, nullptr);
    const std::string raw_path = std::string(kOutDir) + "\\donation.raw";
    const std::string wav_path = std::string(kOutDir) + "\\donation.wav";

    if (!WriteAll(raw_path, &pcm[0], pcm.size() * sizeof(float))) {
        return 1;
    }

    std::vector<int16_t> pcm16(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) {
        pcm16[i] = FloatToPcm16(pcm[i]);
    }
    std::vector<unsigned char> wav = MakeWavHeader(
        static_cast<uint32_t>(pcm16.size()), static_cast<uint32_t>(sample_rate));
    const unsigned char* pcm16_bytes = reinterpret_cast<const unsigned char*>(&pcm16[0]);
    wav.insert(wav.end(), pcm16_bytes, pcm16_bytes + pcm16.size() * sizeof(int16_t));
    if (!WriteAll(wav_path, &wav[0], wav.size())) {
        return 1;
    }

    // ---- 결과 ----
    const double audio_s = static_cast<double>(pcm.size()) / sample_rate;
    std::printf("chunks           : %d\n", num_chunks);
    std::printf("sample rate      : %d Hz\n", sample_rate);
    std::printf("samples          : %zu (float32 mono)\n", pcm.size());
    std::printf("TTFA             : %.1f ms\n", ttfa_ms);
    std::printf("synthesis        : %.1f ms\n", synth_ms);
    std::printf("audio duration   : %.3f s\n", audio_s);
    std::printf("RTF              : %.4f\n", synth_ms / 1000.0 / audio_s);
    std::printf("\nwrote %s (%zu bytes, float32 PCM)\n", raw_path.c_str(),
                pcm.size() * sizeof(float));
    std::printf("wrote %s (%zu bytes, 16-bit PCM WAV)\n", wav_path.c_str(), wav.size());
    return 0;
}
