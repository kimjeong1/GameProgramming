/*
 * [이론 설명: DirectX 11 헤더 의존성]
 * 1. d3d11.h: 핵심 API (Device, Context 등)
 * 2. dxgi.h: 화면 출력, 스왑 체인 등 저수준 하드웨어 인터페이스 관리
 * 3. d3dcompiler.h: HLSL 셰이더 코드를 바이너리로 컴파일하는 기능
 * 
 * glfw 라이브러리 연결안돼서 실행안됨
 */
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>

 // GLFW가 Win32 네이티브 기능(HWND 등)을 노출하도록 설정
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// 라이브러리 링크 (Pragma 사용으로 프로젝트 설정 간소화)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// [이론 설명: 정점 데이터 구조]
// CPU 측의 구조체 데이터는 GPU 측의 Input Layout과 바이트 단위로 일치해야 함.
struct Vertex {
    float x, y, z;      // 위치 (12바이트)
    float r, g, b, a;   // 색상 (16바이트)
};

/*
 * [이론 설명: HLSL 셰이더와 시맨틱]
 * POSITION, COLOR 등은 '시맨틱(Semantic)'이라 불리며,
 * Input Layout에서 정의한 데이터가 어떤 변수로 들어갈지 결정하는 이름표 역할을 함.
 */
const char* shaderSource = R"(
struct VS_INPUT {
    float3 pos : POSITION; // 시맨틱을 통해 위치 데이터 매핑
    float4 col : COLOR;    // 시맨틱을 통해 색상 데이터 매핑
};
struct PS_INPUT {
    float4 pos : SV_POSITION; // SV_는 System Value의 약자로, GPU가 예약한 특수 용도
    float4 col : COLOR;
};

// Vertex Shader: 정점 변환 처리
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}

// Pixel Shader: 최종 색상 결정
float4 PS(PS_INPUT input) : SV_Target {
    return input.col;
}
)";

int main() {
    if (!glfwInit()) return -1;

    // --- 1. GLFW 및 네이티브 윈도우 설정 ---
    // DX11을 직접 쓸 것이므로 GLFW의 기본 API(OpenGL) 사용을 끔
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW + DirectX 11 Triangle", nullptr, nullptr);
    if (!window) return -1;

    // DX11 SwapChain 생성을 위해 Win32 OS가 관리하는 윈도우 핸들(HWND)을 가져옴
    HWND hwnd = glfwGetWin32Window(window);

    // --- 2. DirectX 11 핵심 객체 선언 ---
    // ID3D11Device: 리소스(버퍼, 텍스처, 셰이더) 생성 담당
    // ID3D11DeviceContext: 생성된 리소스로 그리기 명령 실행 담당
    // IDXGISwapChain: 더블 버퍼링 및 화면 출력 제어
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;

    // [이론 설명: 스왑 체인 설정]
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;                                    // 백버퍼 개수
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // 색상 포맷 (RGBA 32비트)
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // 버퍼 용도 (출력용)
    scd.OutputWindow = hwnd;                                // 출력할 윈도우 핸들
    scd.SampleDesc.Count = 1;                               // MSAA(안티앨리어싱) 설정
    scd.Windowed = TRUE;                                    // 창모드 여부

    // 디바이스와 스왑 체인을 동시에 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &context);

    // [이론 설명: 렌더 타겟 뷰(RTV)]
    // DX11에서 리소스에 접근하려면 'View' 객체가 필요함. 
    // 스왑 체인의 백버퍼를 가져와 그곳을 렌더링 타겟으로 지정함.
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release(); // 더 이상 직접 참조는 필요 없으므로 카운트 감소

    // --- 3. 셰이더 컴파일 및 생성 ---
    ID3DBlob* vsBlob, * psBlob; // 컴파일된 바이너리를 담을 블롭(Blob) 객체
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

    // [이론 설명: 인풋 레이아웃(Input Layout)]
    // CPU 정점 구조체(Vertex)와 HLSL 정점 입력(VS_INPUT)을 연결하는 브리지.
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        // 시맨틱명, 인덱스, 데이터 포맷, 슬롯, 오프셋, 분류, 인스턴스 데이터 계수
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}, // 위치(12바이트) 이후 시작
    };
    ID3D11InputLayout* inputLayout;
    device->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    // --- 4. 정점 버퍼(Vertex Buffer) 생성 ---
    Vertex vertices[] = {
        { 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, // 상단 (빨강)
        { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, // 우하단 (초록)
        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}  // 좌하단 (파랑)
    };

    ID3D11Buffer* vertexBuffer;
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼임을 명시
    D3D11_SUBRESOURCE_DATA srd = { vertices };
    device->CreateBuffer(&bd, &srd, &vertexBuffer);

    // --- 5. 메인 루프 ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // [이론 설명: 화면 클리어]
        // 매 프레임마다 이전 그림을 지우고 시작함.
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        context->ClearRenderTargetView(renderTargetView, clearColor);

        // [이론 설명: 출력 병합(Output Merger) 설정]
        // 렌더링 결과를 어느 타겟 뷰로 보낼지 결정함.
        context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        // 뷰포트 설정: 렌더링이 수행될 창의 영역 정의
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);

        // [이론 설명: 입력 어셈블러(Input Assembler) 설정]
        context->IASetInputLayout(inputLayout);
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 기본 도형은 삼각형

        // 셰이더 바인딩
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);

        // 그리기 명령: 정점 3개를 사용하여 드로우콜 수행
        context->Draw(3, 0);

        // [이론 설명: Present]
        // 백버퍼의 내용을 전면 버퍼로 스왑하여 화면에 노출 (수직 동기화 1 프레임 대기)
        swapChain->Present(1, 0);
    }

    // --- 6. 자원 해제 (COM 인터페이스는 Release 호출 필수) ---
    // 생성한 순서의 역순으로 해제하는 것이 안전함.
    vertexBuffer->Release();
    inputLayout->Release();
    vertexShader->Release();
    pixelShader->Release();
    renderTargetView->Release();
    swapChain->Release();
    context->Release();
    device->Release();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}