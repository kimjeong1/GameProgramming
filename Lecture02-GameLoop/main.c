/*
================================================================================
 [핵심 개념: 게임 루프 (Game Loop) - 엔진의 심장]
================================================================================

자료 출처 : https://gameprogrammingpatterns.com/game-loop.html (Game Proggraming Pattern 교재 작성자 블로그)

 1. 게임 루프란 무엇인가?
    - 게임이 실행되는 동안 무한히 반복되며 '입력-업데이트-출력'을 처리하는 설계 패턴.
    - 일반 응용 프로그램(Word, Excel)은 사용자의 입력이 있을 때만 반응하지만,
      게임은 사용자가 가만히 있어도 배경이 움직이고 적이 공격해와야 하므로 '자발적 루프'가 필요함.

 2. 왜 이 패턴이 등장했는가? (이벤트 기반 vs 실시간 루프)
    - 과거 응용 프로그램 방식: "입력이 올 때까지 무한정 대기한다." (Event-driven)
    - 게임 방식: "입력이 있든 없든, 최대한 빨리 세상을 업데이트하고 화면을 다시 그린다."
    - 이 '멈추지 않는 속성' 덕분에 우리는 살아있는 것 같은 실시간 게임 세상을 만날 수 있음.

 3. 게임 루프의 3단계 책임 (Role)
    (1) Process Input: "사용자가 무엇을 했는가?"
        - 메시지 큐를 비우거나 하드웨어 상태를 스캔하여 '의도'를 파악함.
    (2) Update Game State: "세상의 법칙을 적용하면 어떻게 변하는가?"
        - 물리 계산, AI 판단, 충돌 체크 등 모든 '데이터'를 계산함. (그래픽 작업 안 함!)
    (3) Render Frame: "변한 세상을 시각화하라!"
        - 계산 완료된 데이터를 GPU에 전달하여 화면에 그림. (로직 계산 안 함!)

 4. 왜 단계를 나누는가? (관심사의 분리)
    - 입력, 로직, 출력을 섞어버리면 코드가 복잡해져 유지보수가 불가능해짐.
    - 특히 렌더링(출력)은 가장 무거운 작업이므로, 로직(업데이트)과 분리해야
      나중에 '멀티 쓰레딩'이나 '최적화'를 적용하기 유리함.

 5. 게임 엔진의 진화
    - 유니티(Unity)의 Update(), 언리얼(Unreal)의 Tick() 함수들이 바로
      이 거대한 게임 루프 안에서 매 프레임 호출되는 조각들임.
================================================================================


          [Start Game]
                |
                v
+ ---------------------------+
|   1. Process Input         | //키보드, 마우스 등 사용자의 개입 확인
+ ---------------------------+
                |
                v
+ ---------------------------+
|   2. Update Game State     | //캐릭터 이동, 충돌 계산, AI 로직 수행
+ ---------------------------+
                |
                v
+ ---------------------------+
|   3. Render Frame          | //계산된 데이터를 화면에 시각화
+ ---------------------------+
                |                                                                      
      [isRunning == true ? ] ------------> [1. Process Input 으로] //반복(Loop)                                                       
                |                                                                      
                v                                                                      
           [End Game] 
*/

#include <stdio.h>

/*
* 게임의 모든 상태를 담는 바구니 (추상화)
================================================================================

1. 현재 방식(초기 단계)
- GameContext가 playerPos, isRunning 같은 모든 변수를 직접 들고 있음.
- 문제점: 오브젝트가 100개, 1000개로 늘어나면 구조체가 감당할 수 없을 정도로 커짐.

2. 추후 변화될 방식(GameObject & Component 도입)
- GameContext는 이제 구체적인 데이터를 들지 않음.
- 대신 "게임 세상에 존재하는 모든 물체(GameObject)의 리스트"를 관리함.
- [비유] : 바구니에 사과, 포도를 직접 담는 게 아니라, 사과 상자와 포도 상자의 '주소'만 적어두는 것.

3. 컴포넌트 패턴의 핵심(Has - A 관계)
- GameObject는 텅 빈 껍데기임.
- 여기에 Transform(위치), Mesh(외형), Script(로직) 등의 '부품(Component)'을 끼워 넣음.
- Update 단계에서는 Context가 리스트를 돌며 "모든 오브젝트의 모든 컴포넌트를 업데이트하라"고 명령만 함.

4. 구조적 설계(C++ STL이나 Linked List 활용)
- std::vector<GameObject*> objects; // 모든 물체를 담는 바구니
-이 리스트 하나만 있으면, 새로운 적이 생기거나 아이템이 떨어져도 코드 수정 없이 대응 가능함.
================================================================================
*/
typedef struct {
    int playerPos;
    int isRunning;
    char currentInput;
} GameContext;

// --- 1. 입력 단계 (Process Input) ---
// 정석: "무슨 일이 일어났는지"만 기록함.
void ProcessInput(GameContext* ctx) {
    printf("\n[A:왼쪽, D:오른쪽, Q:종료] 입력하세요: ");
    // 공백을 넣어 이전 버퍼를 비우고 입력을 받음
    scanf_s(" %c", &(ctx->currentInput), (unsigned int)sizeof(char));
}

// --- 2. 업데이트 단계 (Update) ---
// 정석: 입력된 정보를 바탕으로 "세상의 법칙"을 적용함.
void Update(GameContext* ctx) {
    // 종료 판정
    if (ctx->currentInput == 'q' || ctx->currentInput == 'Q') {
        ctx->isRunning = 0;
        return;
    }

    // 데이터 변화 (로직)
    if (ctx->currentInput == 'a' || ctx->currentInput == 'A') {
        ctx->playerPos--;
    }
    else if (ctx->currentInput == 'd' || ctx->currentInput == 'D') {
        ctx->playerPos++;
    }

    // 세상의 규칙 (Boundary Check)
    if (ctx->playerPos < 0) ctx->playerPos = 0;
    if (ctx->playerPos > 10) ctx->playerPos = 10;
}

// --- 3. 출력 단계 (Render) ---
// 정석: 현재 데이터 상태를 있는 그대로 "그리기"만 함.
void Render(GameContext* ctx) {
    // 화면 초기화 흉내 (실제 게임에서는 매 프레임 화면을 지움)
    printf("\n\n\n\n\n");

    printf("========== GAME SCREEN ==========\n");
    printf(" Player Position: %d\n", ctx->playerPos);
    printf(" [");
    for (int i = 0; i <= 10; i++) {
        if (i == ctx->playerPos) printf("P");
        else printf("_");
    }
    printf("]\n");
    printf("=================================\n");
}

int main() {
    // 초기화 (Initialization)
    GameContext game = { 5, 1, ' ' };

    printf("게임을 시작합니다. (정석 루프 패턴)\n");

    // [ 정석 게임 루프 ]
    while (game.isRunning) {
        // 1. 입력: 사용자가 무엇을 했는가?
        ProcessInput(&game);

        // 2. 업데이트: 그 결과 세상은 어떻게 변했는가?
        Update(&game);

        // 3. 출력: 변한 세상을 화면에 그려라!
        Render(&game);
    }

    printf("게임이 안전하게 종료되었습니다.\n");
    return 0;
}