/*
================================================================================
 [핵심 개념: Delta Time (Δt) - 게임 엔진의 시간 동기화]
================================================================================

 1. 'Delta(Δ)'의 어원과 의미
    - 수학/물리에서 그리스 문자 Δ(Delta)는 '차이' 또는 '변화량'을 의미함.
    - 게임에서 Delta Time(Δt) = (현재 프레임 시간 - 이전 프레임 시간).
    - 즉, "한 프레임을 실행하는 데 걸린 실제 시간 간격"을 뜻함.

 2. 왜 Delta Time이 필요한가? (프레임 독립성: Frame Rate Independence)
    - 성능이 다른 컴퓨터(슈퍼컴 vs 똥컴)에서 게임 속도는 동일해야 함.
    - [틀린 방식]: Position += Speed;
      -> 1초에 100번 루프 도는 컴은 100만큼 이동, 10번 도는 컴은 10만큼 이동 (불공평!)
    - [옳은 방식]: Position += Speed * DeltaTime;
      -> 루프 횟수가 달라도 '실제 흐른 시간'을 곱해주면 1초 뒤 이동 거리는 정확히 같아짐.

 3. 물리적 비유 (현실 세계의 동기화)
    - 시속 60km로 달리는 차는 1시간(Δt) 동안 60km를 가고, 1분(Δt) 동안 1km를 감.
    - Delta Time은 우리 게임 속 물체가 '현실의 시간 흐름'에 맞춰 움직이게 만드는 마법의 숫자임.

 4. 데이터 타입과 단위
    - 단위: 주로 '초(Second)' 단위를 사용함 (예: 0.016s).
    - 타입: 소수점 아래 정밀도가 중요하므로 float 또는 double을 사용함.

 5. 구현 시 주의사항 (Clamping)
    - 컴퓨터가 순간적으로 렉(Lag)이 걸려 DeltaTime이 너무 커지면(예: 1초 이상),
      물체가 벽을 뚫고 텔레포트하는 버그가 생길 수 있음.
    - 그래서 엔진에서는 보통 dt = min(dt, 0.1f) 정도로 최대치를 제한하는 처리를 함.
================================================================================
*/

#include <iostream>
#include <chrono> // C++ 표준 시간 라이브러리
#include <thread>

/*
 * [C++ 스타일 강의 포인트 1: std::chrono의 구조]
 * - time_point: 특정 시점 (오늘 12시 00분 01초)
 * - duration: 시간의 양 (5초 동안)
 * - clock: 시간의 기준 (steady_clock: 게임처럼 역전되지 않는 일정한 시계)
 */

class CPPGameTimer {
public:
    CPPGameTimer() {
        // 현재 시점으로 초기화
        prevTime = std::chrono::steady_clock::now();
    }

    // 매 프레임마다 호출하여 델타 타임을 반환함.
    float Update() {
        // 1. 현재 시점 기록
        auto currentTime = std::chrono::steady_clock::now();

        /*
         * [C++ 스타일 강의 포인트 2: Duration 계산]
         * - 두 시점(time_point)을 빼면 기간(duration)이 나옴.
         * - duration_cast를 통해 우리가 원하는 단위(초, 밀리초 등)로 변환함.
         */
        std::chrono::duration<float> frameTime = currentTime - prevTime;

        // 2. 계산된 시간 간격을 멤버 변수에 저장
        deltaTime = frameTime.count();

        // 3. 이전 시점 갱신
        prevTime = currentTime;

        return deltaTime;
    }

    float GetDeltaTime() const { return deltaTime; }

private:
    std::chrono::steady_clock::time_point prevTime;
    float deltaTime = 0.0f;
};

int main() {
    CPPGameTimer timer;

    std::cout << "C++ STL chrono 타이머 테스트 시작" << std::endl;

    for (int i = 0; i < 10; ++i) {
        float dt = timer.Update();
        std::cout << "프레임 간격(dt): " << dt << "s" << std::endl;

        // 테스트를 위해 의도적으로 루프를 지연시킴
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}