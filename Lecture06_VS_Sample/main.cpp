/*
================================================================================
 [HLSL 핵심 강의: 상수 버퍼(cbuffer)와 레지스터(register)]
================================================================================

 1. 왜 이런 복잡한 구조를 쓰는가? (The Necessity)
    - CPU와 GPU는 물리적으로 떨어진 별개의 장치임.
    - CPU가 관리하는 메모리(RAM)와 GPU가 관리하는 메모리(VRAM)는 서로 주소가 다름.
    - 따라서 CPU에서 계산한 값(예: 위치 오프셋)을 GPU에게 전달하려면,
      양쪽이 약속한 '특정한 입구'가 필요한데 그게 바로 [register]임.

 2. cbuffer (Constant Buffer) : "명령서 박스"
    - GPU는 데이터를 하나씩 받는 것보다 뭉텅이로 받는 것을 좋아함.
    - 관련된 변수들(위치, 색상, 시간 등)을 하나의 '박스'에 담아 보내는 규격이 cbuffer임.
    - [주의]: GPU는 데이터를 16바이트(float4) 단위로 읽기 때문에,
      박스 내부의 데이터 크기는 항상 16의 배수가 되도록 맞춰야 함 (Padding).

 3. register(b#) : "입구 번호"
    - GPU 하드웨어에는 데이터를 꽂는 여러 종류의 선반(Slot)이 있음.
    - 'b'는 Constant Buffer 전용 선반을 의미함.
    - '0'은 0번 선반이라는 뜻임.
    - [연결 고리]:
        (C++) VSSetConstantBuffers(0, ...) <---> (HLSL) register(b0)
      두 번호가 일치해야만 CPU가 보낸 박스가 셰이더의 변수로 배달됨.

 4. 현대 그래픽스의 특징 (Programmable Pipeline)
    - 과거(DX9 이전)에는 GPU가 정해진 연산만 했기에 이런 지정이 필요 없었음.
    - 현대 GPU는 우리가 시키는 대로만 계산하는 '범용 계산기'이므로,
      데이터의 입구(register)와 모양(cbuffer)을 우리가 직접 설계해줘야 함.
================================================================================


// [실습 예제용 구조]
cbuffer MoveBuffer : register(b0) // 0번 상수 버퍼 입구에서 대기
{
    float2 g_Offset;  // x, y 축으로 얼만큼 이동할지 (8바이트)
    float2 g_Padding; // 16바이트 규격을 맞추기 위한 빈 공간 (8바이트)
};


 [한 줄 요약]
 cbuffer는 CPU가 GPU에게 보내는 '편지 봉투'이고,
 register(b0)는 그 편지가 도착할 '우체통 번호'라고 생각하자.
*/

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h> // 행렬 및 벡터 연산용
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// [1. 상수 버퍼 구조체] 
// HLSL의 cbuffer와 1:1로 매칭되어야 함 (16바이트 정렬 주의)
struct ConstantData {
    XMFLOAT2 offset;    // x, y 이동값 (8바이트)
    float padding[2];   // 16바이트 단위를 맞추기 위한 빈 공간 (8바이트)
};

struct VideoConfig {
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
    bool NeedsResize = false;
    int VSync = 1;
} g_Config;

// 전역 변수
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr; // 상수 버퍼 추가

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// [실시간 이동을 위한 오프셋 변수]
XMFLOAT2 g_CurOffset = { 0.0f, 0.0f };

void RebuildVideoResources(HWND hWnd) {
    if (!g_pSwapChain) return;
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    if (!g_Config.IsFullscreen) {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }
    g_Config.NeedsResize = false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11MoveClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(L"DX11MoveClass", L"Arrows: Move | 1, 2: Resize",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    RebuildVideoResources(hWnd);

    // [셰이더 소스: 오프셋 적용 버전]
    const char* shaderSource = R"(
        cbuffer MoveBuffer : register(b0) // 상수 버퍼 슬롯 b0 사용
        {
            float2 g_Offset; // CPU에서 보내준 x, y 이동값
            float2 g_Padding;
        };

        struct VS_INPUT {
            float3 pos : POSITION;
            float4 col : COLOR;
        };

        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float4 col : COLOR;
        };

        PS_INPUT VS_Main(VS_INPUT input) {
            PS_INPUT output;
            
            // 입력받은 정점 위치에 오프셋을 더함 (이동 처리)
            float3 finalPos = input.pos;
            finalPos.x += g_Offset.x;
            finalPos.y += g_Offset.y;

            output.pos = float4(finalPos, 1.0f);
            output.col = input.col;
            return output;
        }

        float4 PS_Main(PS_INPUT input) : SV_Target {
            return input.col;
        }
    )";

    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS_Main", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS_Main", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release(); psBlob->Release();

    Vertex vertices[] = {
        {  0.0f,  0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.3f, -0.3f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    // [2. 상수 버퍼 생성]
    D3D11_BUFFER_DESC cbd = { 0 };
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantData); // 16바이트
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [방향키 입력 처리]
            float moveSpeed = 0.005f;
            if (GetAsyncKeyState(VK_LEFT))  g_CurOffset.x -= moveSpeed;
            if (GetAsyncKeyState(VK_RIGHT)) g_CurOffset.x += moveSpeed;
            if (GetAsyncKeyState(VK_UP))    g_CurOffset.y += moveSpeed;
            if (GetAsyncKeyState(VK_DOWN))  g_CurOffset.y -= moveSpeed;

            if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
            if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }
            if (g_Config.NeedsResize) RebuildVideoResources(hWnd);

            // [렌더링 시작]
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // [3. 상수 버퍼 데이터 업데이트 및 전송]
            ConstantData cbData = { g_CurOffset };
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cbData, 0, 0);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer); // VS의 0번 슬롯(b0)에 바인딩

            D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
            g_pImmediateContext->Draw(3, 0);

            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}