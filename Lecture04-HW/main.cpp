//lecture4-HW
// 콘솔 서브시스템에서 Win32 창을 함께 사용하기 위한 설정
// - /subsystem:console 환경(기본)에서 WinMain 대신 main()을 진입점으로 씀
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <vector>
#include <string>

// ============================================================
//  전역 화면 설정 (고정 해상도)
// ============================================================
const int SCREEN_W = 800;
const int SCREEN_H = 600;

// ============================================================
//  DirectX 11 전역 인터페이스 포인터
//  - 여러 함수에서 공유해야 하므로 전역으로 선언
// ============================================================
ID3D11Device* g_pd3dDevice = nullptr; // GPU와 통신하는 논리적 장치
ID3D11DeviceContext* g_pImmediateContext = nullptr; // GPU에 명령을 내리는 컨텍스트
IDXGISwapChain* g_pSwapChain = nullptr; // 전면/후면 버퍼를 교체해 화면에 출력
ID3D11RenderTargetView* g_pRenderTargetView = nullptr; // 렌더링 결과가 기록될 대상 뷰
ID3D11VertexShader* g_pVertexShader = nullptr; // 정점(Vertex)을 처리하는 셰이더
ID3D11PixelShader* g_pPixelShader = nullptr; // 픽셀 색상을 결정하는 셰이더
ID3D11InputLayout* g_pInputLayout = nullptr; // GPU에 정점 데이터 형식을 알려주는 레이아웃

// 전체화면 상태 플래그 (F키 토글)
bool g_IsFullscreen = false;

// ============================================================
//  정점(Vertex) 구조체
//  - GPU에 넘길 삼각형 꼭짓점 하나의 데이터
// ============================================================
struct Vertex
{
    float x = 0.0f; // 클립 공간 x 좌표 (-1.0 - 1.0)
    float y = 0.0f; // 클립 공간 y 좌표 (-1.0 - 1.0)
    float z = 0.0f; // 클립 공간 z 좌표 (0.0 - 1.0)
    float r = 1.0f; // 색상 R (0.0 - 1.0)
    float g = 1.0f; // 색상 G (0.0 - 1.0)
    float b = 1.0f; // 색상 B (0.0 - 1.0)
    float a = 1.0f; // 색상 A (투명도, 0.0 - 1.0)
};

// ============================================================
//  HLSL 셰이더 소스 (인라인 문자열)
//  - 별도 .hlsl 파일 없이 런타임에 컴파일함
// ============================================================
const char* g_ShaderSource = R"(
    struct VS_INPUT
    {
        float3 pos : POSITION;
        float4 col : COLOR;
    };
    struct PS_INPUT
    {
        float4 pos : SV_POSITION;
        float4 col : COLOR;
    };

    // 정점 셰이더: 각 꼭짓점마다 한 번 실행됨
    PS_INPUT VS(VS_INPUT input)
    {
        PS_INPUT output;
        output.pos = float4(input.pos, 1.0f);
        output.col = input.col;
        return output;
    }

    // 픽셀 셰이더: 각 픽셀마다 한 번 실행됨
    float4 PS(PS_INPUT input) : SV_Target
    {
        return input.col;
    }
)";

// ============================================================
//  - Component 안에서 소유자 포인터 타입으로 쓰이므로 미리 선언
// ============================================================
class GameObject;

// ============================================================
//  [추상 클래스] Component
//  - 모든 기능 컴포넌트는 이 클래스를 상속받아 구현
// ============================================================
class Component
{
public:
    // 이 컴포넌트를 소유한 GameObject를 역참조할 포인터
    // - GameObject::AddComponent()에서 자동으로 설정됨
    GameObject* pOwner = nullptr;

    // Start() 호출 여부 플래그
    // - false이면 첫 Update 직전에 Start()를 한 번만 호출 (Unity 방식)
    bool isStarted = false;

    // 초기화 함수 (순수 가상) - 객체 생성 시 딱 한 번 실행
    virtual void Start() = 0;

    // 로직 처리 함수 (순수 가상) - 매 프레임 실행
    // dt: 이전 프레임과 현재 프레임 사이의 시간 간격 (초 단위)
    virtual void Update(float dt) = 0;

    // 렌더링 함수 (선택적 오버라이드) - 매 프레임 실행
    virtual void Render() {}

    // 다형성 소멸을 위한 가상 소멸자
    virtual ~Component() {}
};

// ============================================================
//  [GameObject 클래스]
//  - 게임 세계에 존재하는 객체 하나를 표현
//  - 위치(x, y)를 직접 소유
//  - 여러 Component를 부착해 기능을 조합 (컴포넌트 패턴)
// ============================================================
class GameObject
{
public:
    std::string name = ""; // 디버깅용 이름

    // NDC(Normalized Device Coordinates) 기준 위치
    // - x, y 범위: -1.0 ~ 1.0
    float x = 0.0f;
    float y = 0.0f;

    // 부착된 컴포넌트 목록
    // - 메모리 소유권은 GameObject가 가짐 -> 소멸자에서 delete 처리
    std::vector<Component*> components;

    // 생성자: 이름만 받아 초기화
    explicit GameObject(const std::string& n) : name(n) {}

    // 소멸자: 부착된 모든 컴포넌트 메모리 해제
    ~GameObject()
    {
        for (Component* comp : components)
            delete comp;
        components.clear();
    }

    // 컴포넌트를 이 오브젝트에 부착
    // - pComp->pOwner를 자신(this)으로 설정해서 역참조를 가능하게 함
    void AddComponent(Component* pComp)
    {
        pComp->pOwner = this;
        pComp->isStarted = false; // 아직 Start() 미호출 상태
        components.push_back(pComp);
    }

    // 부착된 모든 컴포넌트의 Update를 순서대로 호출
    void Update(float dt)
    {
        for (Component* comp : components)
        {
            // isStarted가 false이면 Update보다 먼저 Start()를 한 번 호출
            if (!comp->isStarted)
            {
                comp->Start();
                comp->isStarted = true;
            }
            comp->Update(dt);
        }
    }

    // 부착된 모든 컴포넌트의 Render를 순서대로 호출
    void Render()
    {
        for (Component* comp : components)
            comp->Render();
    }
};

// ============================================================
//  [RendererComponent 클래스]  Component를 상속
//  - 삼각형 하나를 GPU에 그리는 역할
//  - 소유자(pOwner)의 위치를 매 프레임 읽어서 삼각형을 이동
// ============================================================
class RendererComponent : public Component
{
public:
    // GPU에 올려둔 정점 버퍼 (삼각형 꼭짓점 3개)
    ID3D11Buffer* pVertexBuffer = nullptr;

    // 삼각형 세 꼭짓점의 색상값 (생성자에서 외부 주입)
    float r1 = 1.0f, g1 = 0.0f, b1 = 0.0f; // 꼭짓점 0번
    float r2 = 0.0f, g2 = 1.0f, b2 = 0.0f; // 꼭짓점 1번
    float r3 = 0.0f, g3 = 0.0f, b3 = 1.0f; // 꼭짓점 2번

    // 생성자: 세 꼭짓점의 색상값을 받아 저장
    RendererComponent(
        float R1, float G1, float B1,
        float R2, float G2, float B2,
        float R3, float G3, float B3)
        : r1(R1), g1(G1), b1(B1)
        , r2(R2), g2(G2), b2(B2)
        , r3(R3), g3(G3), b3(B3)
    {
    }

    // Start(): GPU 정점 버퍼를 한 번만 생성
    void Start() override
    {
        // 초기 정점은 위치 0,0 기준으로 잡아둠
        Vertex verts[3];
        verts[0] = { 0.0f,   0.1f,  0.5f, r1, g1, b1, 1.0f };
        verts[1] = { 0.08f, -0.1f,  0.5f, r2, g2, b2, 1.0f };
        verts[2] = { -0.08f,-0.1f,  0.5f, r3, g3, b3, 1.0f };

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(verts);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = verts;

        g_pd3dDevice->CreateBuffer(&bd, &initData, &pVertexBuffer);
    }

    // Update(): 소유자의 현재 위치를 반영해 정점 버퍼를 갱신
    void Update(float dt) override
    {
        (void)dt;

        // 기존 버퍼 파괴
        if (pVertexBuffer)
        {
            pVertexBuffer->Release();
            pVertexBuffer = nullptr;
        }

        // 새 위치로 정점 재계산
        float ox = pOwner->x;
        float oy = pOwner->y;

        Vertex verts[3];
        verts[0] = { ox + 0.0f,   oy + 0.1f,  0.5f, r1, g1, b1, 1.0f };
        verts[1] = { ox + 0.08f,  oy - 0.1f,  0.5f, r2, g2, b2, 1.0f };
        verts[2] = { ox - 0.08f,  oy - 0.1f,  0.5f, r3, g3, b3, 1.0f };

        // 버퍼 새로 생성
        // Usage를 DEFAULT로 바꿈
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(verts);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0; 

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = verts;

        g_pd3dDevice->CreateBuffer(&bd, &initData, &pVertexBuffer);
    }

    // Render(): GPU에 정점 버퍼를 바인딩하고 삼각형 드로우 호출
    void Render() override
    {
        if (!pVertexBuffer) return;

        UINT stride = sizeof(Vertex); // 정점 하나의 크기 (바이트)
        UINT offset = 0;

        // 정점 버퍼를 입력 슬롯 0번에 바인딩
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);

        // 삼각형 설정
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 정점 3개로 삼각형 1개를 그림
        g_pImmediateContext->Draw(3, 0);
    }

    // 소멸자: GPU 버퍼 메모리 해제
    ~RendererComponent() override
    {
        if (pVertexBuffer)
        {
            pVertexBuffer->Release();
            pVertexBuffer = nullptr;
        }
    }
};

// ============================================================
//  [InputComponent 클래스]  Component를 상속
//  - 키보드 입력을 받아 소유자(GameObject)의 위치를 이동
//  - 어떤 키를 사용할지 생성자에서 주입받음 -> 재사용 가능한 구조
// ============================================================
class InputComponent : public Component
{
public:
    // 조작에 사용할 가상 키코드 (VK_UP, 'W' 등)
    int keyUp = 0;
    int keyDown = 0;
    int keyLeft = 0;
    int keyRight = 0;

    // 이동 속도 (NDC 단위 / 초)
    // - NDC 전체 범위가 -1 ~ 1 = 2.0이므로 1.2면 약 1.6초에 화면 횡단
    float speed = 1.0f;

    // 생성자: 조작 키와 속도를 외부에서 주입받음
    InputComponent(int up, int down, int left, int right, float spd)
        : keyUp(up), keyDown(down), keyLeft(left), keyRight(right), speed(spd)
    {
    }

    // Start(): 이 컴포넌트는 별도 초기화 불필요
    void Start() override {}

    // Update(): 키 상태를 체크하고 소유자의 위치를 갱신
    // - [핵심 공식] Position += Velocity * DeltaTime
    // - dt를 곱해야 프레임 속도에 관계없이 일정한 속도로 이동함
    void Update(float dt) override
    {
        // GetAsyncKeyState: 함수 호출 시점의 키 눌림 상태를 즉시 반환
        // - 0x8000 비트가 1이면 해당 키가 현재 눌려 있음
        if (GetAsyncKeyState(keyUp) & 0x8000) pOwner->y += speed * dt; // 위: y 증가
        if (GetAsyncKeyState(keyDown) & 0x8000) pOwner->y -= speed * dt; // 아래: y 감소
        if (GetAsyncKeyState(keyLeft) & 0x8000) pOwner->x -= speed * dt; // 왼쪽: x 감소
        if (GetAsyncKeyState(keyRight) & 0x8000) pOwner->x += speed * dt; // 오른쪽: x 증가

        // 화면 밖으로 나가지 못하도록 NDC 범위 내로 클램프
        if (pOwner->x < -0.9f) pOwner->x = -0.9f;
        if (pOwner->x > 0.9f) pOwner->x = 0.9f;
        if (pOwner->y < -0.9f) pOwner->y = -0.9f;
        if (pOwner->y > 0.9f) pOwner->y = 0.9f;
    }
};

// ============================================================
//  [GameLoop 클래스]
//  - 게임의 메인 루프를 관리
//  - 루프 한 번 = [DeltaTime 계산 - Input - Update - Render]
// ============================================================
class GameLoop
{
public:
    bool isRunning = false; // 루프 계속 여부 (false가 되면 루프 종료)
    std::vector<GameObject*> gameWorld; // 세계에 존재하는 모든 GameObject

    // 시간 관련 변수
    std::chrono::high_resolution_clock::time_point prevTime; // 이전 프레임 시각
    float deltaTime = 0.0f; // 현재 프레임 간격 (초 단위)

    // 초기화: 루프 시작 전 한 번 호출
    void Initialize()
    {
        isRunning = true;
        gameWorld.clear();
        // 현재 시각을 기록해야 첫 프레임 DeltaTime 계산이 가능함
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    // 입력 단계: 시스템 종료(ESC)와 전체화면 토글(F) 처리
    void Input()
    {
        // ESC: 즉시 루프 종료 플래그 설정
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            isRunning = false;

        // F: 전체화면/창 모드 토글
        // - 0x0001 비트: 이번 폴링에서 처음 눌린 경우에만 1 -> 연속 입력 방지
        if (GetAsyncKeyState('F') & 0x0001)
        {
            g_IsFullscreen = !g_IsFullscreen;
            g_pSwapChain->SetFullscreenState(g_IsFullscreen, nullptr);
        }
    }

    // 업데이트 단계: 모든 GameObject와 Component의 Update 호출
    void Update()
    {
        for (GameObject* obj : gameWorld)
            obj->Update(deltaTime);
    }

    // 렌더링 단계: 화면을 지우고 모든 오브젝트를 그림
    void Render()
    {
        // 배경색 (어두운 네이비 계열)
        float clearColor[4] = { 0.05f, 0.05f, 0.15f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

        // 뷰포트 설정: 렌더링 대상 영역 (화면 전체)
        D3D11_VIEWPORT vp = {};
        vp.Width = (float)SCREEN_W;
        vp.Height = (float)SCREEN_H;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        g_pImmediateContext->RSSetViewports(1, &vp);

        // 렌더 타겟을 출력 병합기(OM) 단계에 바인딩
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

        // 셰이더와 입력 레이아웃을 파이프라인에 바인딩
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);

        // 모든 오브젝트의 Render() 호출
        for (GameObject* obj : gameWorld)
            obj->Render();

        // 후면 버퍼 -> 전면 버퍼로 교체하여 화면에 출력
        // Present(1, 0): VSync 활성화 (모니터 주사율에 동기화)
        g_pSwapChain->Present(1, 0);
    }

    // 메인 게임 루프 실행
    // - PeekMessage 방식: 메시지가 없어도 블록되지 않고 즉시 게임 로직을 실행
    // - (비교) GetMessage는 메시지가 올 때까지 블록됨 -> 게임 루프에 부적합
    void Run(HWND hWnd)
    {
        MSG msg = {};
        while (isRunning)
        {
            // A. Win32 메시지 처리 (창 닫기, 키보드 이벤트 등)
            //    PM_REMOVE: 처리 후 메시지 큐에서 제거
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    isRunning = false;
                    break;
                }
                TranslateMessage(&msg); // 가상 키 -> 문자 메시지 변환
                DispatchMessage(&msg);  // 해당 창의 WndProc으로 메시지 전달
            }
            else
            {
                // B. DeltaTime 계산
                //    high_resolution_clock: 나노초 단위의 고해상도 타이머
                auto currentTime = std::chrono::high_resolution_clock::now();
                // duration<float>: 두 시점의 차이를 float 초 단위로 변환
                std::chrono::duration<float> elapsed = currentTime - prevTime;
                deltaTime = elapsed.count(); // 예: 0.0167 = 약 60 FPS
                prevTime = currentTime;     // 다음 프레임을 위해 현재 시각 저장

                // C. 게임 루프 3단계: Input - Update - Render
                Input();      // 입력 처리 (키 상태 확인)
                Update();     // 로직 갱신 (위치 계산 등)
                Render();     // 화면 출력 (GPU 드로우 콜)
            }
        }
        (void)hWnd; // Run에 hWnd를 받지만 현재는 내부에서 미사용
    }

    // 소멸자: gameWorld에 있는 모든 GameObject 메모리 해제
    ~GameLoop()
    {
        for (GameObject* obj : gameWorld)
            delete obj;
        gameWorld.clear();
    }
};

// ============================================================
//  Win32 창 메시지 처리 콜백 (WndProc)
//  - 운영체제에서 창에 발생하는 이벤트를 처리
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0); // 메시지 큐에 WM_QUIT 전송 -> 루프 종료 유도
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ============================================================
//  DirectX 11 초기화 함수
//  - Device, SwapChain, RenderTargetView, Shader, InputLayout 생성
// ============================================================
bool InitDX11(HWND hWnd)
{
    // --- SwapChain 설정 ---
    // - 스왑체인: 더블 버퍼링 관리 (하나는 화면 출력, 하나는 렌더링 중)
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = SCREEN_W;
    sd.BufferDesc.Height = SCREEN_H;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 8비트 x 4채널
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1; // 멀티샘플링 비활성화
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE; // 창 모드로 시작

    // Device + DeviceContext + SwapChain을 한 번에 생성
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                  // 기본 GPU 어댑터 사용
        D3D_DRIVER_TYPE_HARDWARE, // 하드웨어 가속 사용
        nullptr,
        0,                        // 디버그 플래그 (릴리스: 0)
        nullptr,                  // 피처 레벨 배열 (nullptr = 최적 레벨 자동 선택)
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        nullptr,
        &g_pImmediateContext
    );
    if (FAILED(hr)) return false;

    // --- RenderTargetView 생성 ---
    // - 스왑체인의 후면 버퍼를 가져와서 렌더 타겟으로 감쌈
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (!pBackBuffer) return false;
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰 생성 후 참조 카운트 감소

    // --- 셰이더 컴파일 ---
    ID3DBlob* vsBlob = nullptr; // 컴파일된 정점 셰이더 바이트코드
    ID3DBlob* psBlob = nullptr; // 컴파일된 픽셀 셰이더 바이트코드
    ID3DBlob* errBlob = nullptr; // 컴파일 오류 메시지 (실패 시 내용 확인 가능)

    // 정점 셰이더 컴파일 ("VS" 함수, Shader Model 4.0)
    D3DCompile(g_ShaderSource, strlen(g_ShaderSource), nullptr, nullptr, nullptr,
        "VS", "vs_4_0", 0, 0, &vsBlob, &errBlob);
    if (!vsBlob) return false;

    // 픽셀 셰이더 컴파일 ("PS" 함수, Shader Model 4.0)
    D3DCompile(g_ShaderSource, strlen(g_ShaderSource), nullptr, nullptr, nullptr,
        "PS", "ps_4_0", 0, 0, &psBlob, &errBlob);
    if (!psBlob) { vsBlob->Release(); return false; }

    // 컴파일된 바이트코드로 셰이더 객체 생성
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    // --- InputLayout 생성 ---
    // - GPU에게 정점 버퍼의 각 요소가 어디에 해당하는지 알려주는 레이아웃
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        // { 시맨틱이름, 인덱스, 포맷, 입력슬롯, 바이트오프셋, 데이터분류, 인스턴싱스텝 }
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // x, y, z (12바이트)
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // r, g, b, a (오프셋 12)
    };
    g_pd3dDevice->CreateInputLayout(layoutDesc, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    // 바이트코드 블롭은 InputLayout 생성 후 불필요 -> 해제
    vsBlob->Release();
    psBlob->Release();

    return true;
}

// ============================================================
//  DirectX 11 리소스 정리 함수
//  - 생성 역순으로 Release() 호출 -> 메모리 누수 방지
// ============================================================
void CleanupDX11()
{
    // 전체화면 상태라면 창 모드로 복귀 후 해제 (DXGI 권장 사항)
    if (g_pSwapChain) g_pSwapChain->SetFullscreenState(FALSE, nullptr);

    if (g_pInputLayout) { g_pInputLayout->Release();      g_pInputLayout = nullptr; }
    if (g_pPixelShader) { g_pPixelShader->Release();      g_pPixelShader = nullptr; }
    if (g_pVertexShader) { g_pVertexShader->Release();     g_pVertexShader = nullptr; }
    if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pImmediateContext) { g_pImmediateContext->Release(); g_pImmediateContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

// ============================================================
//  main(): 프로그램 진입점
//  - 콘솔 서브시스템(/subsystem:console)에서는 WinMain 대신 main()을 사용
//  - GetModuleHandle(NULL)로 현재 프로세스의 hInstance를 직접 얻어옴
// ============================================================
int main()
{
    // GetModuleHandle(NULL): WinMain의 hInstance 파라미터와 동일한 값을 반환
    // - 현재 실행 중인 .exe 모듈의 핸들 (창 클래스 등록 및 창 생성에 필요)
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // --- 1. Win32 창 클래스 등록 ---
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 기본 배경 브러시
    wcex.lpszClassName = L"DX11GameClass";
    RegisterClassExW(&wcex);

    // --- 2. 창 생성 ---
    // AdjustWindowRect: 클라이언트 영역이 SCREEN_W x SCREEN_H가 되도록 창 크기 계산
    RECT rc = { 0, 0, SCREEN_W, SCREEN_H };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(
        L"DX11GameClass",
        L"DX11 Component System | Arrow: Player1 | WASD: Player2 | F: Fullscreen | ESC: Exit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) return -1;
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    // --- 3. DirectX 11 초기화 ---
    if (!InitDX11(hWnd))
    {
        MessageBoxW(hWnd, L"DirectX 11 초기화 실패", L"오류", MB_OK);
        return -1;
    }

    // --- 4. 게임 루프 초기화 ---
    GameLoop gLoop;
    gLoop.Initialize();

    // --- 5. Player1 GameObject 조립 ---
    // - 방향키(VK_UP/DOWN/LEFT/RIGHT)로 조작
    // - 빨강/주황/노랑 삼각형
    {
        GameObject* player1 = new GameObject("Player1");
        player1->x = -0.5f; // 초기 위치: 화면 왼쪽
        player1->y = 0.0f;

        // 렌더러 컴포넌트: 꼭짓점 색상 지정
        player1->AddComponent(new RendererComponent(
            1.0f, 0.2f, 0.2f,  // 꼭짓점 0: 빨강
            1.0f, 0.6f, 0.0f,  // 꼭짓점 1: 주황
            1.0f, 1.0f, 0.0f   // 꼭짓점 2: 노랑
        ));

        // 입력 컴포넌트: 방향키, 속도 1.2 (NDC/초)
        player1->AddComponent(new InputComponent(
            VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, 1.2f
        ));

        gLoop.gameWorld.push_back(player1);
    }

    // --- 6. Player2 GameObject 조립 ---
    // - WASD 키로 조작
    // - 하늘색/파랑/보라 삼각형
    {
        GameObject* player2 = new GameObject("Player2");
        player2->x = 0.5f; // 초기 위치: 화면 오른쪽
        player2->y = 0.0f;

        // 렌더러 컴포넌트: 꼭짓점 색상 지정
        player2->AddComponent(new RendererComponent(
            0.0f, 0.8f, 1.0f,  // 꼭짓점 0: 하늘색
            0.2f, 0.2f, 1.0f,  // 꼭짓점 1: 파랑
            0.7f, 0.2f, 1.0f   // 꼭짓점 2: 보라
        ));

        // 입력 컴포넌트: WASD, 속도 1.2 (NDC/초)
        player2->AddComponent(new InputComponent(
            'W', 'S', 'A', 'D', 1.2f
        ));

        gLoop.gameWorld.push_back(player2);
    }

    // --- 7. 게임 루프 실행 ---
    // - ESC를 누르거나 창을 닫을 때까지 루프가 계속 돌아감
    gLoop.Run(hWnd);

    // --- 8. DirectX 리소스 정리 ---
    // - GameLoop 소멸자가 gameWorld의 메모리를 해제
    // - DirectX COM 객체는 별도로 해제 필요
    CleanupDX11();

    return 0;
}