#pragma once

using std::string;
using std::cout;
using Clock = std::chrono::steady_clock;

namespace {

	// 기본 경로. argv 로 덮어쓸 수 있다.
	constexpr const char* kDefaultModel =
		"C:\\Dev\\40TTS\\PiperTest\\voices\\ko_KR-kss-medium.onnx";
	constexpr const char* kDefaultConfig =
		"C:\\Dev\\40TTS\\PiperTest\\voices\\ko_KR-kss-medium.onnx.json";
	constexpr const char* kDefaultEspeakData =
		"C:\\Dev\\40TTS\\PiperTest\\third_party\\piper1-gpl\\libpiper\\install\\share\\espeak-ng-data";
	constexpr const char* kOutDir = "out";


	double MsSince(Clock::time_point start) {
		return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	}

	static string ToUtf8(const wchar_t* input)
	{
		if (input == nullptr)
		{
			return string();
		}

		const int input_size = WideCharToMultiByte(
			CP_UTF8,
			0,
			input,
			-1,
			nullptr,
			0,
			nullptr,
			nullptr
		);

		if (input_size <= 1)
		{
			return string();
		}

		string output(input_size - 1, ' ');

		WideCharToMultiByte(
			CP_UTF8,
			0,
			input,
			-1,
			output.data(), // &output[0]
			input_size,
			nullptr,
			nullptr
		);

		return output;
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



