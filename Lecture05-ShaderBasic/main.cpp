/*
================================================================================
 [HLSL: High-Level Shading Language]
================================================================================

 1. HLSL이란 무엇인가? (Definition)
    - Microsoft에서 DirectX API를 위해 개발한 '고수준 셰이딩 언어'임.
    - 과거에는 어셈블리어(저수준)로 GPU를 제어했으나, 인간이 이해하기 쉬운
      C언어 문법으로 발전시킨 것이 바로 HLSL임.
    - OpenGL 진영의 GLSL, NVIDIA의 Cg와 같은 뿌리를 가진 형제 격 언어임.

 2. 역사적 배경 (History & Context)
    - [고정 함수 파이프라인 시절]: 초기 GPU는 정해진 연산(이동, 조명 등)만 할 수 있었음.
      개발자는 값만 던졌을 뿐, 내부 연산 과정을 수정할 수 없었음.
    - [셰이더 모델의 등장]: 2000년대 초반, DirectX 8.0과 함께 '프로그래밍 가능한
      파이프라인'이 등장함. 이때부터 개발자가 GPU 내부 계산식을 직접 짤 수 있게 됨.
    - [현대적 의미]: 이제 셰이더는 단순히 색을 칠하는 도구가 아니라, 물리 연산,
      포스트 프로세싱, 레이트레이싱 등 현대 게임 그래픽의 90% 이상을 담당함.

 3. 셰이더의 두 기둥 (The Two Pillars)
    - Vertex Shader (정점 셰이더):
        * 점(Vertex) 하나하나를 처리함.
        * "이 점이 화면 어디에 찍혀야 하는가?" (공간 변환)를 결정함.
    - Pixel Shader (픽셀 셰이더):
        * 화면의 픽셀(Pixel) 하나하나를 처리함.
        * "이 픽셀이 최종적으로 무슨 색이어야 하는가?" (라이팅, 텍스처링)를 결정함.

 4. 핵심 용어 및 주의사항 (Core Concepts)
    - Semantic (시맨틱): 변수 뒤의 ': POSITION' 같은 것들. GPU와 CPU 간의 약속임.
    - SV_ (System Value): 'SV_POSITION'처럼 SV가 붙으면 GPU 시스템이 직접
      관리하는 특수 변수라는 뜻임.
    - Precision (정밀도): float(32비트), half(16비트) 등을 통해 연산 효율을 조절함.
================================================================================
*/


/*
================================================================================
 [그래픽스 파이프라인(Graphics Pipeline) 전체 공정 도식]
================================================================================

[ CPU (C++ Code) ] --- (데이터 전송: Vertex Buffer, Constant Buffer) ---+
                                                                        |
                                                                        v
                                    +---------------------------------------------------------------------------+
                                    |  [1. Input Assembler (IA)] - 입구/분류                                    |
                                    |  - 메모리 조각들을 모아서 '점(Vertex)' 단위로 조립함.                     |
                                    |  - InputLayout을 보고 "이건 위치, 이건 색상"이라고 라벨을 붙임.           |
                                    +----------------------+----------------------------------------------------+
                                                                        |
                                                                        v
                                    +----------------------+----------------------------------------------------+
                                    |  [2. Vertex Shader (VS)] - 정점 처리 (가공) <--- **프로그래밍 가능**      |
                                    |  - 각 점의 위치를 계산함. (Local -> World -> View -> Projection 변환)     |
                                    |  - 행렬(Matrix)을 곱해서 물체를 이동, 회전, 크기 조절함.                  |
                                    +----------------------+----------------------------------------------------+
                                                                        |
                                                                        v
                                    +----------------------+----------------------------------------------------+
                                    |  [3. Rasterizer (RS)] - 픽셀화 (기하 -> 비트맵)                           |
                                    |  - 점 3개를 연결해서 삼각형 면을 만들고, 그 안을 채울 '픽셀'들을 찾아냄.  |
                                    |  - 화면 밖의 정점을 잘라내고(Clipping), 안 보이는 면을 제거함(Culling).   |
                                    |  - **중요**: 정점 사이의 데이터를 '보간(Interpolation)'해서 픽셀에 넘겨줌.|
                                    +----------------------+----------------------------------------------------+
                                                                        |
                                                                        v
                                    +----------------------+----------------------------------------------------+
                                    |  [4. Pixel Shader (PS)] - 색상 처리 (채색) <--- **프로그래밍 가능**       |
                                    |  - 결정된 각 픽셀의 최종 색상을 계산함.                                   |
                                    |  - 조명(Lighting), 텍스처(Texture), 그림자 효과를 여기서 처리함.          |
                                    +----------------------+----------------------------------------------------+
                                                                        |
                                                                        v
                                    +----------------------+----------------------------------------------------+
                                    |  [5. Output Merger (OM)] - 최종 병합/출력                                 |
                                    |  - 픽셀 셰이더가 만든 색상을 '백버퍼(도화지)'에 실제로 그림.              |
                                    |  - 깊이 테스트(Depth Test)를 통해 앞뒤 관계를 따져서 가려진 곳을 처리함.  |
                                    +---------------------------------------------------------------------------+
                                                                        |
                                                                        v
             [ Monitor (Front Buffer) ] <--- (Present 호출) --- [ Back Buffer ]

================================================================================
*/


#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// [비디오 설정 관리 구조체]
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

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// --- [해상도 및 리소스 재구성 함수] ---
void RebuildVideoResources(HWND hWnd) {
    if (!g_pSwapChain) return;

    // 1. 기존 렌더 타겟 뷰 해제 (안 하면 ResizeBuffers 실패함)
    if (g_pRenderTargetView) {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }

    // 2. 백버퍼 크기 재설정
    g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);

    // 3. 새 백버퍼로부터 렌더 타겟 뷰 다시 생성
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 4. 윈도우 창 크기 실제 조정 (전체화면이 아닐 때만)
    if (!g_Config.IsFullscreen) {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }

    g_Config.NeedsResize = false;
    printf("[Video] Changed: %d x %d\n", g_Config.Width, g_Config.Height);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 클래스 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11VideoClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(L"DX11VideoClass", L"F: Fullscreen | 1: 800x600 | 2: 1280x720",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. DX11 Device & SwapChain 생성
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

    // 3. 초기 리소스 빌드
    RebuildVideoResources(hWnd);

    // 4. 셰이더 컴파일 (간략화된 소스 사용)
    /*
    ================================================================================
     [1. 그래픽스 파이프라인과 구조체의 관계]
    ================================================================================
     1. CPU (C++ 코드) -> Input Assembler (IA):
        - 우리가 만든 정점 버퍼(Vertex Buffer)가 '공장 입구'에 도착함.

     2. VS_INPUT (정점 셰이더 입력):
        - 입구에 도착한 원자재를 '정점 셰이더'라는 1번 공정 라인에 올리는 단계임.
        - 그래서 "입력 데이터(VS_INPUT)"가 필요함.

     3. PS_INPUT (픽셀 셰이더 입력):
        - 1번 공정(VS)을 마친 데이터가 '래스터라이저(Rasterizer)'라는 중간 기계를 거쳐
          2번 공정(PS)으로 넘어가는 단계임.
        - 이때 GPU는 점 3개를 보고 그 사이를 수만 개의 픽셀로 채우는 '마법(보간)'을 부림.
        - 이 마법을 부린 결과물을 담기 위해 "출력/전달 데이터(PS_INPUT)"가 필요함.
    
    ================================================================================
         [2. Semantic(시맨틱)은 왜 필요한가? (The Role of Semantics)]
    ================================================================================
     1. 데이터의 신분증 (Identity):
        - HLSL 변수 이름(pos, col 등)은 개발자가 마음대로 지을 수 있음.
        - 하지만 GPU는 그 이름만 보고는 이게 좌표인지 색상인지 모름.
        - ': POSITION'이라고 꼬리표를 붙여줘야 "아, 이건 좌표니까 연산 장치로 보내자!"라고 판단함.

        [핵심 질문: InputLayout의 시맨틱과 HLSL의 시맨틱은 똑같아야 하는가?]
         a. 정답: "반드시 똑같아야 함 (Must Match)."
            - InputLayout은 CPU가 GPU에게 주는 '물품 목록표'임.
            - HLSL의 VS_INPUT은 GPU가 받는 '택배 상자'임.
            - 목록표에 'POSITION'이라고 적혔는데, 받는 상자에 'MY_POS'라고 적혀 있으면
              GPU는 "내가 찾는 물건이 아니네?" 하고 데이터를 버려버림 (렌더링 안 됨).

         b. 연결 고리 (The Link):
            - [C++ 코드]
              { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, ... }
                 ▲ 이 문자열이 바로 '키(Key)'임.

            - [HLSL 코드]
              float3 pos : POSITION;
                           ▲ 이 꼬리표가 '키(Key)'임.

            - 두 키가 일치해야만 비로소 메모리상의 데이터가 `pos` 변수로 흘러 들어감.

         c. 만약 정의되지 않은 시맨틱을 쓴다면?
            - 에러가 나거나, 값이 0(검정색 혹은 원점)으로 들어옴.
            - 반대로 InputLayout에는 있는데 HLSL에서 안 쓰면? 그건 괜찮음 (남는 물건 처리).

     2. 하드웨어 유닛과의 연결 (Hardware Link):
        - GPU 내부에는 '좌표 계산 전용 유닛', '색상 처리 유닛' 등이 나뉘어 있음.
        - 시맨틱은 각 데이터를 올바른 하드웨어 처리 장치로 배달하는 '주소' 역할을 함.

     3. 시스템 예약어 (System Values):
        - 'SV_'가 붙은 시맨틱(SV_POSITION 등)은 하드웨어가 특별히 관리하는 값임.
        - "이 값은 화면 어디에 찍힐지 최종 결정된 좌표니까 래스터라이저한테 바로 넘겨!"라는 뜻임.
    ================================================================================
    */
    const char* shaderSource = R"(
        

        // [1. 입력 데이터 구조체]
        // CPU(C++ 코드)에서 보낸 정점 데이터가 처음으로 도착하는 입구임.
        // C++의 Vertex 구조체와 데이터 순서, 형식이 반드시 일치해야 함.
        struct VS_INPUT
        {
            float3 pos : POSITION; // 위치 정보 (x, y, z) : 꼬리표 POSITION
            float4 col : COLOR;    // 색상 정보 (r, g, b, a) : 꼬리표 COLOR
        };

        // [2. 출력/전달 데이터 구조체]
        // Vertex Shader가 계산을 마치고 Pixel Shader에게 넘겨줄 구조체임.
        struct PS_INPUT
        {
            // SV_POSITION: System Value의 약자. 
            // GPU가 "아, 이게 최종적으로 화면 어디에 찍힐 좌표구나!"라고 인식하게 함. (필수)
            float4 pos : SV_POSITION; 
            float4 col : COLOR;        // 색상은 그대로 전달함.
        };

        // -----------------------------------------------------------------------------
        // [3. Vertex Shader (VS): 정점 처리 단계]
        // - 역할: 각 점(Vertex)의 위치를 결정함. 
        // - 특징: 삼각형의 점이 3개라면 이 함수는 총 3번 실행됨.
        // -----------------------------------------------------------------------------
        PS_INPUT VS_Main(VS_INPUT input)
        {
            PS_INPUT output;

            // 3D 좌표(float3)를 4D 좌표(float4)로 확장함.
            // 마지막 1.0f(w값)는 행렬 연산과 투영을 위해 필요함.
            output.pos = float4(input.pos, 1.0f);

            // 색상은 딱히 계산할 게 없으므로 들어온 대로 통과시킴 (Pass-through)
            output.col = input.col;

            return output;
        }

        // -----------------------------------------------------------------------------
        // [4. Pixel Shader (PS): 픽셀 처리 단계]
        // - 역할: 화면에 찍힐 점 하나하나의 '최종 색상'을 결정함.
        // - 특징: 삼각형 내부를 채우는 수만 개의 픽셀 수만큼 실행됨 (가장 연산량이 많음).
        // -----------------------------------------------------------------------------
        // SV_Target: 이 함수가 리턴하는 값이 '현재 렌더 타겟(도화지)'에 그려질 색상임을 뜻함.
        float4 PS_Main(PS_INPUT input) : SV_Target
        {
            // 정점 셰이더에서 넘겨받은 색상을 그대로 반환함.
            // (만약 여기서 return float4(1, 0, 0, 1); 이라고 쓰면 모든 삼각형이 빨갛게 나옴)
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

    // 5. 삼각형 버퍼 생성
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    // --- [게임 루프] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [입력 처리: GetAsyncKeyState의 0x0001 플래그로 1회성 입력 감지]
            if (GetAsyncKeyState('F') & 0x0001) {
                g_Config.IsFullscreen = !g_Config.IsFullscreen;
                g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
            }
            if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
            if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }

            // [해상도 변경 적용]
            if (g_Config.NeedsResize) RebuildVideoResources(hWnd);

            // [렌더링]
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // 중요: 뷰포트는 매 프레임 바뀐 g_Config 기준으로 설정
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

            g_pSwapChain->Present(g_Config.VSync, 0); // V-Sync 활성화
        }
    }

    // [정리]
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