/*
================================================================================
 [핵심 개념: 공간(BackBuffer)과 범위(Viewport)의 분리]
================================================================================
 1. ID3D11Texture2D (백버퍼 리소스)
    - 실제 메모리 점유율을 결정하는 물리적 해상도임.

 2. D3D11_VIEWPORT (뷰포트 설정)
    - 래스터라이저(Rasterizer) 단계에서 "도화지의 어디를 쓸 것인가"를 결정함.
    - 백버퍼 해상도보다 작게 설정하면 화면의 일부분에만 그림이 그려짐.
    - 이를 통해 화면 분할(Split Screen)이나 PIP(Picture-in-Picture)를 구현함.

 3. IDXGISwapChain::Present (화면 출력)
    - 백버퍼에 기록된 데이터를 윈도우 창으로 전송하는 최종 단계임.
================================================================================

================================================================================
 [핵심 설계: 해상도 및 뷰포트의 통합 관리]
================================================================================
 1. 윈도우 크기 (Window Size):
    - OS가 관리하는 '창'의 전체 외곽 크기입니다. 타이틀바와 테두리를 포함함.

 2. 백버퍼 해상도 (BackBuffer/Canvas Resolution):
    - GPU 메모리에 할당되는 실제 '도화지'의 픽셀 수.
    - 게임의 실제 그래픽 품질(선명도)을 결정하는 물리적 수치.

 3. 뷰포트 (Viewport):
    - 생성된 백버퍼(도화지) 내에서 "실제로 그림을 채울 영역".
    - 백버퍼가 800x600이라도 뷰포트를 400x300으로 잡으면 화면의 1/4만 사용.
================================================================================
*/

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// [중앙 관리 설정] 엔진의 모든 해상도 기준점
struct VideoConfig {
    const int Width = 800;
    const int Height = 600;
} g_Config;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// (중략: Vertex 구조체 및 셰이더 소스는 동일)

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // --- [1. 윈도우 생성 단계] ---
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11EngineClass";
    RegisterClassExW(&wcex);

    // [중요] AdjustWindowRect: 우리가 원하는 '그림 영역'이 g_Config 크기가 되도록 
    // 타이틀바와 테두리 두께를 계산하여 전체 창 크기를 역산함.
    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"DX11EngineClass", L"통합 해상도 관리 예제",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, // 계산된 창 크기 적용
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);

    // --- [DX11 리소스 생성 단계] ---
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    // [중요] 스왑체인의 버퍼 크기는 곧 '백버퍼'의 물리 해상도임.
    // 중앙 설정값(g_Config)을 사용하여 데이터 일관성을 유지함.
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

   
    // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    /*
     typedef struct D3D11_INPUT_ELEMENT_DESC {
         LPCSTR                     SemanticName;         // 1. 의미 (이름)
         UINT                       SemanticIndex;        // 2. 번호 (인덱스)
         DXGI_FORMAT                Format;               // 3. 데이터 형식 (크기/타입)
         UINT                       InputSlot;            // 4. 입력 슬롯 (통로)
         UINT                       AlignedByteOffset;    // 5. 오프셋 (시작 지점)
         D3D11_INPUT_CLASSIFICATION InputSlotClass;       // 6. 클래스 (데이터 성격)
         UINT                       InstanceDataStepRate; // 7. 인스턴싱 스텝
     } D3D11_INPUT_ELEMENT_DESC;
     */
     //정점의 데이터 형식을 정의 (IA 단계에 알려줌)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성 (삼각형 데이터)
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);




    // --- [게임 루프 단계] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [출력 단계]
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // [중요] 뷰포트 설정
            // 뷰포트는 백버퍼라는 메모리 공간 중 '어디에' 픽셀을 채울지 결정함.
            // 일반적으로 백버퍼 전체를 사용하도록 g_Config 값을 그대로 할당함.
            // 만약 멀티뷰(분할화면)를 한다면 이 Width/Height를 절반으로 줄여서 사용함.
            
            D3D11_VIEWPORT vp;
            vp.TopLeftX = 0;                                //뷰포트 시작 X 좌표
            vp.TopLeftY = 0;                                // 뷰포트 시작 Y 좌표
            vp.Width = (float)g_Config.Width;               // 뷰포트 가로 폭
            vp.Height = (float)g_Config.Height;             // 뷰포트 세로 폭
            vp.MinDepth = 0.0f;                             // 깊이 버퍼의 최소값 (0.0 ~ 1.0)
            vp.MaxDepth = 1.0f;                             // 깊이 버퍼의 최대값
            g_pImmediateContext->RSSetViewports(1, &vp);

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            g_pImmediateContext->Draw(3, 0);

            // [중요] Present의 역할
            // 다 그려진 백버퍼(g_Config 크기)를 실제 윈도우 창으로 전송함.
            // 윈도우 창 크기와 백버퍼 크기가 다르면 여기서 '강제 스케일링'이 발생하여 화질이 깨짐.
            g_pSwapChain->Present(0, 0);

            /* Present(a,b)
            * 첫 번째 인자 (a): SyncInterval (V-Sync 설정)
            *      - 0:     즉시 출력. 모니터가 화면을 갱신하든 말든 상관없이 다 그렸으면 바로 화면을 교체함. (FPS가 무제한으로 올라가며 화면 찢어짐 발생 가능)
            *      - 1~4:   수직 동기화(V-Sync) 활성화. 
            *               1은 모니터 주사율(60Hz 등)에 맞춰서 기다렸다가 화면 스왑.
            *               2/3/4 는 모니터가 화면을 2/3/4번 그릴 때마다 스왑
            * 두 번째 인자 (b): Flags (출력 옵션)
            *      - 0:                         일반적인 출력임.
            *        DXGI_PRESENT_TEST:         실제로 화면을 바꾸지는 않고, 장치가 준비되었는지 테스트만 할 때 씀.
            *        DXGI_PRESENT_DO_NOT_WAIT:  만약 GPU가 이전 프레임을 처리하느라 바쁘면 기다리지 않고 바로 에러를 뱉게 함.
            */
        }
    }

    // 생성(Create)한 모든 객체는 프로그램 종료 전 반드시 Release 해야 함.
    // 생성의 역순으로 해제하는 것이 관례임.
    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}




