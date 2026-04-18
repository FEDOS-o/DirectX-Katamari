#pragma once
#include "Game.h"
#include <wincodec.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "windowscodecs.lib")

namespace Core {

    class TextureLoader {
    public:
        static ID3D11ShaderResourceView* LoadTexture2D(Game* game, const std::string& path, bool silent = false) {
            if (!game || !game->Device) return nullptr;

            std::ifstream file(path);
            if (!file.good()) {
                if (!silent) {
                    MessageBoxA(nullptr, ("Texture not found: " + path).c_str(), "Error", MB_OK);
                }
                return nullptr;
            }
            file.close();

            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            ID3D11ShaderResourceView* textureView = nullptr;

            Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
            hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
                std::wstring wpath(path.begin(), path.end());
                hr = wicFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

                if (SUCCEEDED(hr)) {
                    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                    hr = decoder->GetFrame(0, &frame);

                    if (SUCCEEDED(hr)) {
                        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                        hr = wicFactory->CreateFormatConverter(&converter);

                        if (SUCCEEDED(hr)) {
                            hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

                            if (SUCCEEDED(hr)) {
                                UINT width, height;
                                converter->GetSize(&width, &height);

                                std::vector<BYTE> pixels(width * height * 4);
                                hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());

                                if (SUCCEEDED(hr)) {
                                    D3D11_TEXTURE2D_DESC texDesc = {};
                                    texDesc.Width = width;
                                    texDesc.Height = height;
                                    texDesc.MipLevels = 1;
                                    texDesc.ArraySize = 1;
                                    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                    texDesc.SampleDesc.Count = 1;
                                    texDesc.Usage = D3D11_USAGE_DEFAULT;
                                    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                                    D3D11_SUBRESOURCE_DATA initData = {};
                                    initData.pSysMem = pixels.data();
                                    initData.SysMemPitch = width * 4;

                                    ID3D11Texture2D* tex = nullptr;
                                    hr = game->Device->CreateTexture2D(&texDesc, &initData, &tex);

                                    if (SUCCEEDED(hr) && tex) {
                                        hr = game->Device->CreateShaderResourceView(tex, nullptr, &textureView);
                                        tex->Release();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            CoUninitialize();
            return textureView;
        }

        static ID3D11ShaderResourceView* LoadCubeTexture(Game* game, const std::string& crossPath, int& outFaceSize) {
            if (!game || !game->Device) return nullptr;

            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            ID3D11ShaderResourceView* cubeTextureView = nullptr;
            outFaceSize = 0;

            Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
            hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
                std::wstring wpath(crossPath.begin(), crossPath.end());
                hr = wicFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

                if (SUCCEEDED(hr)) {
                    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                    hr = decoder->GetFrame(0, &frame);

                    if (SUCCEEDED(hr)) {
                        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                        hr = wicFactory->CreateFormatConverter(&converter);

                        if (SUCCEEDED(hr)) {
                            hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

                            if (SUCCEEDED(hr)) {
                                UINT width, height;
                                converter->GetSize(&width, &height);
                                outFaceSize = width / 4;

                                std::vector<BYTE> fullPixels(width * height * 4);
                                hr = converter->CopyPixels(nullptr, width * 4, (UINT)fullPixels.size(), fullPixels.data());

                                if (SUCCEEDED(hr)) {
                                    std::vector<std::vector<BYTE>> faceData(6);
                                    for (int i = 0; i < 6; i++) {
                                        faceData[i].resize(outFaceSize * outFaceSize * 4);
                                    }

                                    // Extract 6 faces from cross layout
                                    // +X (1,2), -X (1,0), +Y (0,1), -Y (2,1), +Z (1,1), -Z (1,3)
                                    int faceRows[] = { 1, 1, 0, 2, 1, 1 };
                                    int faceCols[] = { 2, 0, 1, 1, 1, 3 };

                                    for (int face = 0; face < 6; face++) {
                                        int srcRow = faceRows[face];
                                        int srcCol = faceCols[face];

                                        for (int y = 0; y < outFaceSize; y++) {
                                            int srcY = srcRow * outFaceSize + y;
                                            for (int x = 0; x < outFaceSize; x++) {
                                                int srcX = srcCol * outFaceSize + x;
                                                int srcIdx = (srcY * width + srcX) * 4;
                                                int dstIdx = (y * outFaceSize + x) * 4;
                                                for (int c = 0; c < 4; c++) {
                                                    faceData[face][dstIdx + c] = fullPixels[srcIdx + c];
                                                }
                                            }
                                        }
                                    }

                                    D3D11_TEXTURE2D_DESC texDesc = {};
                                    texDesc.Width = outFaceSize;
                                    texDesc.Height = outFaceSize;
                                    texDesc.MipLevels = 1;
                                    texDesc.ArraySize = 6;
                                    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                    texDesc.SampleDesc.Count = 1;
                                    texDesc.Usage = D3D11_USAGE_DEFAULT;
                                    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                                    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

                                    std::vector<D3D11_SUBRESOURCE_DATA> subresources(6);
                                    for (int i = 0; i < 6; i++) {
                                        subresources[i].pSysMem = faceData[i].data();
                                        subresources[i].SysMemPitch = outFaceSize * 4;
                                        subresources[i].SysMemSlicePitch = outFaceSize * outFaceSize * 4;
                                    }

                                    ID3D11Texture2D* cubeTex = nullptr;
                                    hr = game->Device->CreateTexture2D(&texDesc, subresources.data(), &cubeTex);

                                    if (SUCCEEDED(hr) && cubeTex) {
                                        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                                        srvDesc.Format = texDesc.Format;
                                        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                                        srvDesc.TextureCube.MostDetailedMip = 0;
                                        srvDesc.TextureCube.MipLevels = 1;

                                        hr = game->Device->CreateShaderResourceView(cubeTex, &srvDesc, &cubeTextureView);
                                        cubeTex->Release();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            CoUninitialize();
            return cubeTextureView;
        }
    };

} // namespace Core