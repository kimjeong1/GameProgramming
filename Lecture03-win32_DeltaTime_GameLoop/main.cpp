/*
================================================================================
 [실전 가이드: DeltaTime(dt)을 어떻게 사용해야 하는가?]
================================================================================

 1. 모든 '변화'가 일어나는 곳에 곱하라 (Movement & Rotation)
    - 캐릭터 이동, 회전, 크기 변경 등 '시간에 따라 변하는 것'에는 무조건 dt를 곱함.
    - [공식]: 현재값 += 변화량(속도) * DeltaTime;
    - 이유: 속도의 단위가 '프레임당 이동 거리'에서 '초당 이동 거리'로 바뀌기 때문임.

 2. 물리 법칙의 적용 (Gravity & Acceleration)
    - 중력이나 가속도 역시 시간의 제곱에 비례하거나 시간에 따라 축적됨.
    - [공식]: 속력 += 가속도 * DeltaTime;
             위치 += 속력 * DeltaTime;
    - 이렇게 단계별로 dt를 곱해주면 어떤 PC에서도 동일한 점프 높이와 낙하 속도가 나옴.

 3. 쿨타임 및 타이머 구현 (Cooldowns)
    - "3초 뒤에 스킬 발동" 같은 로직을 구현할 때 스톱워치 역할을 함.
    - [방법]: float timer += DeltaTime;
             if (timer >= 3.0f) { 스킬 발동! }

 4. 보간(Interpolation) 처리 (Lerp)
    - A 지점에서 B 지점으로 부드럽게 이동시킬 때 사용함.
    - t(시간 계수)에 DeltaTime을 누적시켜 부드러운 애니메이션을 만듦.

 5. [주의] 렌더링(Render) 단계에서는 사용하지 마라!
    - DeltaTime은 'Update(로직)' 단계에서 데이터를 계산할 때만 사용함.
    - Render는 계산이 끝난 최종 결과물을 화면에 '그리기'만 해야 함.
      (그리는 도중에 위치를 계산하면 화면이 찢어지는 등의 문제가 생길 수 있음)
================================================================================
*/


/*
================================================================================
 [std::chrono 기반 Variable Delta Time 게임 루프]
================================================================================
 1. std::chrono 사용 이유
    - Win32의 QPC보다 이식성이 좋고, C++ 표준이며, 타입 안정성이 뛰어남.
 2. Update(float dt)의 역할
    - 모든 '변화'는 dt에 의존함. (현재값 += 속도 * dt)
    - dt는 '초(second)' 단위의 실수(float)로 변환하여 사용하는 것이 일반적임.
 3. 왜 Render에는 dt가 없는가?
    - Render는 '현재 상태'를 출력하는 시각화 도구일 뿐임.
    - 로직 계산과 출력을 분리해야 '데이터의 무결성'이 보장됨. (데이터 오염 방지)
================================================================================
*/

#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>

// 데이터 구조
typedef struct {
    float x, y;
    float speed;
} GameObject;

typedef struct {
    bool isUp;
    bool isDown;
    bool isLeft;
    bool isRight;
    bool isExit;
} InputContext;

// --- [게임 엔진 핵심 단계] ---

void ProcessInput(InputContext* context) {
    context->isUp = (GetAsyncKeyState('W') & 0x8000);
    context->isDown = (GetAsyncKeyState('S') & 0x8000);
    context->isLeft = (GetAsyncKeyState('A') & 0x8000);
    context->isRight = (GetAsyncKeyState('D') & 0x8000);
    context->isExit = (GetAsyncKeyState(VK_ESCAPE) & 0x8000);
}

void Update(GameObject* player, const InputContext* context, float dt) {
    if (context->isUp)    player->y -= player->speed * dt;
    if (context->isDown)  player->y += player->speed * dt;
    if (context->isLeft)  player->x -= player->speed * dt;
    if (context->isRight) player->x += player->speed * dt;
}

void Render(const GameObject* player, float fps) {
    system("cls");
    printf("=== Explicit Type Engine ===\n");
    printf("FPS : %.2f | DT : %.4fs\n", fps, 1.0f / fps);
    printf("Player: (%.1f, %.1f)\n", player->x, player->y);
    printf("============================\n");

    int py = (int)(player->y / 10.0f);
    int px = (int)(player->x / 5.0f);
    for (int i = 0; i < py; i++) printf("\n");
    for (int i = 0; i < px; i++) printf(" ");
    printf("★");
}

int main() {
    GameObject player = { 100.0f, 100.0f, 200.0f };
    InputContext input = { 0 };

    // high_resolution_clock 내부에 정의된 time_point 타입을 직접 명시함.
    std::chrono::high_resolution_clock::time_point prevTime = std::chrono::high_resolution_clock::now();

    while (!input.isExit) {
        // A. DeltaTime 계산
        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = currentTime - prevTime; // 두 시점의 차이를 구함 (초 단위 float으로 캐스팅)
        float dt = elapsed.count();
        prevTime = currentTime;

        // B. 루프 실행
        ProcessInput(&input);
        Update(&player, &input, dt);
        Render(&player, 1.0f / dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 1ms 라도 쉬어서 CPU가 버닝하는것을 막음
    }

    return 0;
}