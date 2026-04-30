/*
================================================================================
 [GPU MEMORY HIERARCHY & BINDING QUICK REFERENCE]
================================================================================
 1. 레지스터 (r#): 개인 책상. 함수 내 지역 변수. 너무 많이 쓰면 병렬성 저하.
 2. 그룹 공유 메모리 (g#): 팀 공용 화이트보드. Compute Shader 쓰레드간 공유용.
 3. 상수 버퍼 (b#): 방송 공지사항. 모든 쓰레드가 동시 참조. 16바이트 정렬 필수.
 4. 리소스 뷰 (t#): 읽기 전용 도서관. 텍스처, 구조화 버퍼 등 캐시 최적화됨.
 5. UAV (u#): 공동 작업 칠판. 쓰기 가능 영역. 원자적 연산(Atomic) 필요.
 6. 샘플러 (s#): 돋보기/렌즈. 데이터를 어떻게 필터링(Linear/Point)할지 결정.
================================================================================
 [CAUTION] 
 - Constant Buffer는 반드시 float4(16 bytes) 단위로 데이터가 끊기도록 설계할 것.
 - t#은 빠르지만 수정 불가, u#은 수정 가능하지만 동기화(Race Condition) 주의.
================================================================================
*/

// [3단계: Constant Buffer] - 모든 정점/픽셀이 공통으로 쓸 변환 행렬
// CPU에서 "야, 이번 프레임은 이 카메라 행렬로 그려!"라고 방송 때리는 용도.
cbuffer TransformData : register(b0)
{
    matrix worldMatrix; // 64 bytes
    matrix viewMatrix; // 64 bytes
    matrix projMatrix; // 64 bytes
    float3 lightDirection; // 12 bytes
    float padding; // 4 bytes (16바이트 정렬을 위한 더미 데이터)
};

// [4단계: Resource View] - 텍스처 도서관
Texture2D sideTexture : register(t0);

// [6단계: Sampler State] - 도서관 책을 어떻게 읽을지 결정하는 안경
SamplerState linearSampler : register(s0);

// 셰이더 입출력 구조체 (C 구조체와 동일하게 생각함)
struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL; // 예시를 위해 추가
};

// [Vertex Shader]
// GPU 연산 유닛(ALU)이 각 정점마다 이 함수를 실행함.
PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    
    // [1단계: Temporary Registers] - output 같은 변수들이 여기 생성됨.
    // 좌표 변환: Local -> World -> View -> Projection
    float4 worldPos = mul(float4(input.pos, 1.0f), worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projMatrix);
    
    output.uv = input.uv;
    
    return output;
}

// [Pixel Shader]
float4 PS_Main(PS_INPUT input) : SV_Target
{
    // [1단계: Temporary Registers] - 연산 도중 생기는 임시 값들
    float4 finalColor;

    // [4단계 + 6단계 조합] - 도서관(t0)에서 안경(s0) 쓰고 데이터 읽기
    // 필터링 하드웨어가 여기서 선형 보간(Linear Interpolation)을 공짜로 해줌.
    finalColor = sideTexture.Sample(linearSampler, input.uv);

    // 단순 연산 (개인 책상 위에서 계산)
    // lightDirection은 [3단계: b0]에서 가져온 방송 공지사항임.
    float diffuse = saturate(dot(input.normal, -lightDirection));
    finalColor.rgb *= diffuse;

    return finalColor;
}