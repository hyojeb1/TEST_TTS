#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <piper.h>

//--
#include <iostream>
#include "HyojeHelper.h"

//--


int wmain(int argc, wchar_t* argv[])
{
	::SetConsoleOutputCP(CP_UTF8);
	const string text = (argc> 1) ? ToUtf8(argv[1]) : ToUtf8(L"도네이션 도네이션 도네이션  감사합니다.");
	const string model_path = (argc > 2) ? ToUtf8(argv[2]) : kDefaultModel;
	const string config_path = (argc > 3) ? ToUtf8(argv[3]) : kDefaultConfig;
	const string espeak_data = (argc > 4) ? ToUtf8(argv[4]) : kDefaultEspeakData;

	std::printf("libpiper version : %s\n", piper_version());
	std::printf("model            : %s\n", model_path.c_str());
	std::printf("espeak-ng-data   : %s\n", espeak_data.c_str());
	std::printf("text             : %s\n\n", text.c_str());

	const Clock::time_point t_load = Clock::now();

	// 1. 생성 옵션 준비
	piper_create_options opts;
	piper_init_create_options(&opts);
	opts.model_path = model_path.c_str();
	opts.config_path = config_path.c_str();
	opts.espeak_data_path = espeak_data.c_str(); // 텍스트 음소 처리에 필요

	// 2. 신디사이저 생성
	piper_synthesizer* synth = piper_create_with_options(&opts);
	const double load_ms = MsSince(t_load);
	if (!synth) { 
		std::printf("[FAIL] piper_create returned NULL. " "Check the model, config and espeak-ng-data paths.\n");
		return 1;
	}
	std::printf("model load       : %.1f ms\n", load_ms);

	// 3. 합성 옵션 (기본값 가져와서 필요하면 수정)
	piper_synthesize_options synOpts = piper_default_synthesize_options(synth);
	synOpts.length_scale = 1.0f; // 속도

	// 4. 합성 시작
	const Clock::time_point t_synth = Clock::now();
	int result_synth = piper_synthesize_start(synth, text.c_str(), &synOpts);
	if (result_synth != PIPER_OK){
		std::printf("[FAIL] piper_synthesize_start failed\n");
		piper_free(synth);
		return 1;
	}

	std::vector<float> pcm;
	int sample_rate = 0;
	int num_chunks = 0;
	double ttfa_ms = -1.0;

	// 5. 청크 단위로 오디오 꺼내기 (스트리밍 방식)
	piper_audio_chunk chunk{};
	while (1) {
		const int ret = piper_synthesize_next(synth, &chunk);
		
		if (ret != PIPER_OK && ret != PIPER_DONE){
			cout << "[FAIL] piper_synthesize_next returned " << ret << "\n";
			piper_free(synth);
			return 1;
		}
		
		if (chunk.samples != nullptr && chunk.num_samples > 0){
			if (ttfa_ms < 0.0){
				ttfa_ms = MsSince(t_synth);
			}
			sample_rate = chunk.sample_rate;
			++num_chunks;
			pcm.insert(pcm.end(), chunk.samples, chunk.samples + chunk.num_samples);
		}

		if (ret == PIPER_DONE || chunk.is_last){
			break;
		}
	}

	// 6. 해제
	const double synth_ms = MsSince(t_synth);
	piper_free(synth);

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