//Subsystem: 프로그램의 외형과 환경(콘솔 창 유무).
//Entry Point : 프로그램의 시작점(어떤 함수부터 읽을 것인가).
//DirectX 게임은 보통 WINDOWS를 쓰지만, 디버깅 로그를 편하게 보려고 CONSOLE 환경에서 WinMain을 시작점으로 쓰는 트릭을 자주 사용함.


// pragma comment : 특정 데이터나 주석을 목적 파일(Object file)이나 실행 파일(Executable)에 삽입하라는 구체적인 명령임.

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") // 하위 시스템을 콘솔로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:windows") // 하위 시스템을 윈도우로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/subsystem:console")


//#pragma comment(lib, "d3d11.lib"))  // 외부 라이브러리 사용

#include <windows.h>

// 1. 윈도우 메시지 처리 함수 (Window Procedure)
// 사용자 입력(키보드, 마우스)이나 시스템 이벤트를 처리하는 핵심 콜백 함수임.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        // 창이 닫힐 때 호출됨. 메시지 큐에 WM_QUIT을 넣어 루프를 종료시킴.
        PostQuitMessage(0);
        break;
    default:
        // 직접 처리하지 않은 메시지는 OS의 기본 처리 방식에 맡김.
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 2. WinMain: 프로그램의 진입점 (Entry Point)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // --- (A) 윈도우 클래스 등록 ---
    // 생성할 창의 속성(아이콘, 커서, 배경색, 메시지 처리 함수 등)을 정의함.
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;           // 가로/세로 크기 변경 시 다시 그리기
    wcex.lpfnWndProc = WndProc;                     // 메시지 처리 함수 연결
    wcex.hInstance = hInstance;                     // 현재 프로그램 인스턴스 핸들
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);  // 기본 화살표 커서 사용
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  // 창 배경색 설정
    wcex.lpszClassName = L"DirectXWindowClass";      // 클래스 고유 이름

    RegisterClassExW(&wcex);

    // --- (B) 윈도우 생성 ---
    // 실제 화면에 표시될 윈도우 객체를 생성함
    // Handle to a Window = HWND
    HWND hWnd = CreateWindowW(
        L"DirectXWindowClass",      // 등록한 클래스 이름
        L"DirectX Learning Window", // 창 타이틀 바 제목
        WS_OVERLAPPEDWINDOW,        // 일반적인 윈도우 스타일 (최소화, 최대화 등 포함)
        CW_USEDEFAULT, CW_USEDEFAULT, // 초기 위치 (X, Y)
        800, 600,                   // 창 너비와 높이
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) return FALSE;

    // 생성된 창을 화면에 표시하고 업데이트함.
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // --- (C) 메시지 루프 ---
    // OS로부터 전달되는 메시지를 지속적으로 감시하고 처리함.
    // DirectX 연동 시에는 GetMessage 대신 PeekMessage를 사용하여 무한 루프를 돌림.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); // 키보드 입력 메시지 변환
        DispatchMessage(&msg);  // WndProc으로 메시지 전달
    }

    return (int)msg.wParam;
}