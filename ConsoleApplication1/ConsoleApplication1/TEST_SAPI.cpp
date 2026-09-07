#include <windows.h>
#include <sapi.h>

#pragma comment(lib, "sapi.lib")

int main()
{
    // COM 초기화
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr))
        return 1;

    ISpVoice* pVoice = nullptr;

    hr = CoCreateInstance(
        CLSID_SpVoice,
        nullptr,
        CLSCTX_ALL,
        IID_ISpVoice,
        reinterpret_cast<void**>(&pVoice)
    );

    pVoice->SetRate(5);  // 빠름

    if (SUCCEEDED(hr))
    {
        pVoice->Speak(
            L"안녕하세요. 윈도우 네이티브 티티에스 테스트입니다.",
            SPF_DEFAULT,
            nullptr
        );

        pVoice->Release();
    }

    CoUninitialize();

    return 0;
}