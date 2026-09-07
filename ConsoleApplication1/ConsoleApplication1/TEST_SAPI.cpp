#include <windows.h>
#include <sapi.h>

#pragma warning(push)
#pragma warning(disable : 4996)
#include <sphelper.h>
#pragma warning(pop)

#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "sapi.lib")

int main()
{
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

    if (SUCCEEDED(hr))
    {
        IEnumSpObjectTokens* pEnum = nullptr;

        hr = SpEnumTokens(
            SPCAT_VOICES,
            nullptr,
            nullptr,
            &pEnum
        );

        if (SUCCEEDED(hr))
        {
            ULONG count = 0;
            pEnum->GetCount(&count);

            wprintf(L"Voice count: %lu\n\n", count);

            for (ULONG i = 0; i < count; ++i)
            {
                ISpObjectToken* pToken = nullptr;

                if (pEnum->Next(1, &pToken, nullptr) == S_OK)
                {
                    WCHAR* description = nullptr;

                    if (SUCCEEDED(SpGetDescription(pToken, &description)))
                    {
                        wprintf(L"[%lu] %s\n", i, description);

                        CoTaskMemFree(description);
                    }

                    pToken->Release();
                }
            }

            pEnum->Release();
        }

        pVoice->Release();
    }

    CoUninitialize();

    return 0;
}