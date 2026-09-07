#include <iostream>
#include <io.h>
#include <fcntl.h>

#include <future>
#include <atomic>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "windowsapp")

using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Media::Core;
using namespace Windows::Media::Playback;
using namespace Windows::Storage::Streams;

int wmain()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    // WinRT 초기화
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    try
    {
        SpeechSynthesizer synthesizer;

        // --------------------------------------------------------
        // 설치되어 있는 TTS Voice 출력
        // --------------------------------------------------------
        std::wcout << L"=== Installed Voices ===\n";

        auto voices = SpeechSynthesizer::AllVoices();

        std::wcout
            << L"Windows.Media.SpeechSynthesis -> "
            << voices.Size()
            << L"개\n\n";

        VoiceInformation koreanVoice{ nullptr };

        for (const auto& voice : voices)
        {
            std::wcout
                << L"Name: " << voice.DisplayName().c_str()
                << L" / Language: " << voice.Language().c_str()
                << L"\n";

            if (!koreanVoice && voice.Language() == L"ko-KR")
            {
                koreanVoice = voice;
            }
        }

        // 한국어 Voice가 있으면 사용
        if (koreanVoice)
        {
            synthesizer.Voice(koreanVoice);

            std::wcout
                << L"\nSelected Korean Voice: "
                << koreanVoice.DisplayName().c_str()
                << L"\n";
        }
        else
        {
            std::wcout
                << L"\nKorean voice not found. Using default voice.\n";
        }

        // --------------------------------------------------------
        // TTS 생성
        // --------------------------------------------------------
        const winrt::hstring text =
            L"안녕하세요. 윈도우 미디어 스피치 신세시스 테스트입니다. "
            L"도네이션 천 메소가 도착했습니다.";

        std::wcout << L"\nSynthesizing...\n";

        SpeechSynthesisStream speechStream =
            synthesizer
            .SynthesizeTextToStreamAsync(text)
            .get();

        // --------------------------------------------------------
        // SpeechSynthesisStream -> MediaSource
        // --------------------------------------------------------
        IRandomAccessStream randomAccessStream =
            speechStream.as<IRandomAccessStream>();

        MediaSource mediaSource =
            MediaSource::CreateFromStream(
                randomAccessStream,
                speechStream.ContentType()
            );

        // --------------------------------------------------------
        // 실제 스피커 재생
        // --------------------------------------------------------
        MediaPlayer player;

        std::promise<void> finishedPromise;
        std::future<void> finishedFuture = finishedPromise.get_future();

        std::atomic_bool finished = false;

        auto Complete = [&]()
            {
                bool expected = false;

                if (finished.compare_exchange_strong(expected, true))
                {
                    finishedPromise.set_value();
                }
            };

        // 정상 종료
        auto endedRevoker = player.MediaEnded(
            winrt::auto_revoke,
            [&](MediaPlayer const&, IInspectable const&)
            {
                std::wcout << L"\nPlayback finished.\n";
                Complete();
            }
        );

        // 재생 오류
        auto failedRevoker = player.MediaFailed(
            winrt::auto_revoke,
            [&](MediaPlayer const&, MediaPlayerFailedEventArgs const& args)
            {
                std::wcerr
                    << L"\nPlayback failed: "
                    << args.ErrorMessage().c_str()
                    << L"\n";

                Complete();
            }
        );

        player.Source(mediaSource);

        std::wcout << L"Playing...\n";

        player.Play();

        // 음성이 끝날 때까지 프로그램 종료 방지
        finishedFuture.wait();
    }
    catch (const winrt::hresult_error& e)
    {
        std::wcerr
            << L"WinRT Error: "
            << e.message().c_str()
            << L"\n";

        winrt::uninit_apartment();
        return 1;
    }

    winrt::uninit_apartment();

    return 0;
}