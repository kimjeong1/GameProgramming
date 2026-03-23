/*
 * [셰이더와 상수 버퍼]
 * 1. cbuffer: 셰이더에서 외부 데이터를 받는 바구니 (register b0)
 * 2. D3D11_BIND_CONSTANT_BUFFER: CPU 데이터를 GPU 전용 상수 메모리에 묶음
 * 3. UpdateSubresource: 매 프레임 변하는 위치 값을 GPU로 복사함
 */

 // --- [C++ 쪽 구조체 정의] ---
 // 주의: 16바이트 정렬을 위해 float4(16byte) 단위를 쓰거나 크기를 맞춰야 함.
struct MyConstantBuffer {
    float offsetX;
    float offsetY;
    float padding[2]; // 16바이트 배수를 맞추기 위한 빈 공간 (중요!)
};

// --- [HLSL 셰이더 소스] ---
const char* shaderSource = R"(
cbuffer MyBuffer : register(b0) {
    float offsetX;
    float offsetY;
    float2 padding;
};

struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    // 셰이더 내부에서 위치를 실시간으로 변환함
    output.pos = float4(input.pos.x + offsetX, input.pos.y + offsetY, input.pos.z, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col;
}
)";

// --- [메인 루프 내부의 변화] ---
// ... (초기화 단계에서 g_pd3dDevice->CreateBuffer로 pConstantBuffer 생성)
int main()
{
    // 아직 미완
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // 메시지 처리...
        }
        else {
            // 1. [Update] 입력에 따른 좌표 계산 (가변값)
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  g_posX -= 0.001f;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_posX += 0.001f;

            // 2. [CPU -> GPU 데이터 전송] 핵심!
            MyConstantBuffer cb;
            cb.offsetX = g_posX;
            cb.offsetY = g_posY;
            g_pImmediateContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);

            // 3. [Render] 셰이더에 상수 버퍼 연결
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);

            // ... Clear, Draw, Present 순서로 진행
        }
    }

    return 0;
}