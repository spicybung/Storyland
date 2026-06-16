#include "wic_image.h"

#include <Windows.h>
#include <wincodec.h>
#include <comdef.h>
#include <sstream>
#include <vector>

static std::string hresultText(HRESULT hr) {
    std::ostringstream ss;
    ss << "HRESULT 0x" << std::hex << unsigned(hr);
    return ss.str();
}

class ComInitGuard {
public:
    ComInitGuard() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInitGuard() {
        if (SUCCEEDED(result)) CoUninitialize();
    }
    HRESULT result;
};

template<typename T>
class ComPtrLocal {
public:
    ComPtrLocal() = default;
    ~ComPtrLocal() { reset(); }
    T** put() { reset(); return &ptr; }
    T* get() const { return ptr; }
    T* operator->() const { return ptr; }
    void reset() { if (ptr) { ptr->Release(); ptr = nullptr; } }
private:
    T* ptr = nullptr;
};

bool loadImageWithWic(const std::wstring& path, RgbaImage& image, std::string& errorMessage) {
    ComInitGuard com;
    if (FAILED(com.result) && com.result != RPC_E_CHANGED_MODE) {
        errorMessage = "COM init failed: " + hresultText(com.result);
        return false;
    }

    ComPtrLocal<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()));
    if (FAILED(hr)) {
        errorMessage = "WIC factory failed: " + hresultText(hr);
        return false;
    }

    ComPtrLocal<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.put());
    if (FAILED(hr)) {
        errorMessage = "Could not decode image: " + hresultText(hr);
        return false;
    }

    ComPtrLocal<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.put());
    if (FAILED(hr)) {
        errorMessage = "Could not read image frame: " + hresultText(hr);
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        errorMessage = "Image dimensions are invalid.";
        return false;
    }

    ComPtrLocal<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.put());
    if (FAILED(hr)) {
        errorMessage = "Could not create format converter: " + hresultText(hr);
        return false;
    }

    hr = converter->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        errorMessage = "Could not convert image to RGBA: " + hresultText(hr);
        return false;
    }

    image.width = int(width);
    image.height = int(height);
    image.rgba.resize(size_t(width) * size_t(height) * 4);
    hr = converter->CopyPixels(nullptr, width * 4, UINT(image.rgba.size()), image.rgba.data());
    if (FAILED(hr)) {
        errorMessage = "Could not copy image pixels: " + hresultText(hr);
        return false;
    }
    return true;
}

bool savePngWithWic(const std::wstring& path, const RgbaImage& image, std::string& errorMessage) {
    if (image.width <= 0 || image.height <= 0 || image.rgba.size() != size_t(image.width) * size_t(image.height) * 4) {
        errorMessage = "Image buffer is invalid.";
        return false;
    }

    ComInitGuard com;
    if (FAILED(com.result) && com.result != RPC_E_CHANGED_MODE) {
        errorMessage = "COM init failed: " + hresultText(com.result);
        return false;
    }

    ComPtrLocal<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()));
    if (FAILED(hr)) {
        errorMessage = "WIC factory failed: " + hresultText(hr);
        return false;
    }

    ComPtrLocal<IWICStream> stream;
    hr = factory->CreateStream(stream.put());
    if (FAILED(hr)) {
        errorMessage = "Could not create WIC stream: " + hresultText(hr);
        return false;
    }
    hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        errorMessage = "Could not open PNG output: " + hresultText(hr);
        return false;
    }

    ComPtrLocal<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
    if (FAILED(hr)) {
        errorMessage = "Could not create PNG encoder: " + hresultText(hr);
        return false;
    }
    hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        errorMessage = "Could not initialize PNG encoder: " + hresultText(hr);
        return false;
    }

    ComPtrLocal<IWICBitmapFrameEncode> frame;
    ComPtrLocal<IPropertyBag2> bag;
    hr = encoder->CreateNewFrame(frame.put(), bag.put());
    if (FAILED(hr)) {
        errorMessage = "Could not create PNG frame: " + hresultText(hr);
        return false;
    }
    hr = frame->Initialize(bag.get());
    if (FAILED(hr)) {
        errorMessage = "Could not initialize PNG frame: " + hresultText(hr);
        return false;
    }
    hr = frame->SetSize(UINT(image.width), UINT(image.height));
    if (FAILED(hr)) {
        errorMessage = "Could not set PNG size: " + hresultText(hr);
        return false;
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        errorMessage = "Could not set PNG pixel format: " + hresultText(hr);
        return false;
    }
    hr = frame->WritePixels(UINT(image.height), UINT(image.width * 4), UINT(image.rgba.size()), const_cast<BYTE*>(image.rgba.data()));
    if (FAILED(hr)) {
        errorMessage = "Could not write PNG pixels: " + hresultText(hr);
        return false;
    }
    hr = frame->Commit();
    if (FAILED(hr)) {
        errorMessage = "Could not commit PNG frame: " + hresultText(hr);
        return false;
    }
    hr = encoder->Commit();
    if (FAILED(hr)) {
        errorMessage = "Could not commit PNG encoder: " + hresultText(hr);
        return false;
    }
    return true;
}
