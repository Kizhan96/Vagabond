#include "dxgi_capture.h"
#include <QDebug>

DxgiCapture::DxgiCapture() {}

DxgiCapture::~DxgiCapture() {}

bool DxgiCapture::initialize(int outputIndex) {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   &device, &obtained, &context);
    if (FAILED(hr)) {
        qWarning() << "D3D11CreateDevice failed" << QString::number(hr, 16);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(outputIndex, &output))) {
        qWarning() << "EnumOutputs failed";
        return false;
    }
    output->GetDesc(&outputDesc);

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    output.As(&output1);
    hr = output1->DuplicateOutput(device.Get(), &duplication);
    if (FAILED(hr)) {
        qWarning() << "DuplicateOutput failed" << QString::number(hr, 16);
        duplication.Reset();
        return false;
    }

    return true;
}

bool DxgiCapture::ensureDuplication() {
    if (duplication) return true;
    staging.Reset();
    lastFrame = QImage();
    return initialize();
}

QImage DxgiCapture::grab() {
    // Recreate device/duplication if they were lost
    if (!device || !context) {
        duplication.Reset();
        staging.Reset();
        if (!initialize()) return QImage();
    }
    if (!ensureDuplication()) return QImage();

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
    HRESULT hr = duplication->AcquireNextFrame(50, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        if (lastFrame.isNull()) {
            QSize sz = targetSize;
            if (!sz.isValid() || sz.isEmpty()) {
                sz = QSize(outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left,
                           outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);
            }
            if (!sz.isValid() || sz.isEmpty()) sz = QSize(1, 1);
            QImage blank(sz, QImage::Format_ARGB32);
            blank.fill(Qt::black);
            return blank;
        }
        return lastFrame; // no new frame, keep previous
    }
    if (FAILED(hr)) {
        duplication.Reset();
        return QImage();
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> frameTexture;
    desktopResource.As(&frameTexture);

    D3D11_TEXTURE2D_DESC desc{};
    frameTexture->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    // Validate that texture belongs to our device
    Microsoft::WRL::ComPtr<ID3D11Device> srcDevice;
    frameTexture->GetDevice(&srcDevice);
    if (!srcDevice || srcDevice.Get() != device.Get()) {
        duplication->ReleaseFrame();
        duplication.Reset();
        staging.Reset();
        return QImage();
    }

    if (staging) {
        // Ensure staging texture belongs to the same device and matches size/format
        Microsoft::WRL::ComPtr<ID3D11Device> stagingDevice;
        staging->GetDevice(&stagingDevice);
        if (!stagingDevice || stagingDevice.Get() != device.Get()) {
            staging.Reset();
        } else {
            D3D11_TEXTURE2D_DESC sdesc{};
            staging->GetDesc(&sdesc);
            if (sdesc.Width != desc.Width || sdesc.Height != desc.Height || sdesc.Format != desc.Format) {
                staging.Reset();
            }
        }
    }
    if (!staging) {
        device->CreateTexture2D(&desc, nullptr, &staging);
    }

    context->CopyResource(staging.Get(), frameTexture.Get());
    // CopyResource returns void, Map will fail if something went wrong
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        duplication->ReleaseFrame();
        staging.Reset();
        duplication.Reset();
        return QImage();
    }

    QImage image(desc.Width, desc.Height, QImage::Format_ARGB32);
    const uchar *src = static_cast<const uchar*>(mapped.pData);
    for (UINT y = 0; y < desc.Height; ++y) {
        memcpy(image.scanLine(y), src + y * mapped.RowPitch, desc.Width * 4);
    }
    context->Unmap(staging.Get(), 0);
    duplication->ReleaseFrame();

    if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    lastFrame = image;
    return image;
}
