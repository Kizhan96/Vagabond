#pragma once

#include <QImage>
#include <QSize>
#include <QRect>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_2.h>

class DxgiCapture {
public:
    DxgiCapture();
    ~DxgiCapture();

    bool initialize(int outputIndex = 0);
    QImage grab();
    void resize(int width, int height) { targetSize = QSize(width, height); }

private:
    bool ensureDuplication();

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    DXGI_OUTPUT_DESC outputDesc{};
    QSize targetSize;
    QImage lastFrame;
};
