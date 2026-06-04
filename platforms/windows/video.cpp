module;

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

export module video;

import <iostream>;

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "Mfreadwrite.lib")

export void InitializeCamera() {
    HRESULT hr = MFStartup(MF_VERSION);

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 2);

    hr = pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    IMFSourceReader* pReader = nullptr;

    pAttributes->Release();
}
