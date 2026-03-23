/*
 * [이론 설명: OpenGL 확장 로더와 윈도우 라이브러리]
 * 1. GLEW: OpenGL은 하드웨어 제조사마다 지원 함수 주소가 다르므로, 실행 시점에 함수 포인터를 찾아주는 로더가 필수임.
 * 2. GLFW: OS마다 다른 윈도우 생성, 입력 처리를 통합 관리해주는 멀티플랫폼 라이브러리.
 */
#include <GL/glew.h>    // 확장 로더: 반드시 GLFW보다 먼저 포함해야 함 (함수 프로토타입 선점)
#include <GLFW/glfw3.h> // 윈도우 및 컨텍스트 관리
#include <iostream>

 /*
  * [이론 설명: 그래픽스 파이프라인 - 셰이더]
  * 셰이더는 GPU 내의 작은 프로그램임.
  * - Vertex Shader: 정점의 위치를 결정. (로컬 좌표 -> 클립 좌표 변환)
  * - Fragment Shader: 최종 픽셀의 색상을 결정.
  */

  // Vertex Shader: 정점 데이터를 처리하는 단계
const char* vertexShaderSource = R"(
#version 460 core // OpenGL 4.6 기능을 사용함을 명시
layout (location = 0) in vec3 aPos;   // VBO에서 0번 위치로 들어오는 정점 좌표
layout (location = 1) in vec3 aColor; // VBO에서 1번 위치로 들어오는 정점 색상

layout (location = 0) out vec3 ourColor; // Fragment Shader로 넘겨줄 출력 변수

void main() {
    // gl_Position은 GPU가 예약한 출력 변수임 (정규화된 장치 좌표)
    gl_Position = vec4(aPos, 1.0); 
    ourColor = aColor; // 색상 데이터를 파이프라인 다음 단계로 전달
}
)";

// Fragment Shader: 픽셀의 색상을 처리하는 단계
const char* fragmentShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 ourColor; // Vertex Shader로부터 보간되어 들어온 색상
layout (location = 0) out vec4 FragColor; // 최종 화면에 출력될 색상 (RGBA)

void main() {
    FragColor = vec4(ourColor, 1.0f);
}
)";

int main() {
    // --- 1. 라이브러리 초기화 및 윈도우 생성 ---
    if (!glfwInit()) return -1;

    // OpenGL 컨텍스트 프로파일 설정 (Core Profile은 레거시 기능을 배제함)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL 4.6 with GLEW", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    // 현재 스레드에 OpenGL 컨텍스트를 바인딩 (이 시점부터 OpenGL 명령 실행 가능)
    glfwMakeContextCurrent(window);

    // --- 2. GLEW 초기화 (확장 함수 포인터 로드) ---
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // --- 3. 셰이더 컴파일 및 링크 (GPU 프로그램 준비) ---
    // 각 셰이더 객체를 생성하고 컴파일한 뒤, 하나의 '프로그램'으로 묶는 과정임.
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram); // 최종 셰이더 프로그램 완성

    // 링크 후에는 개별 셰이더 객체는 필요 없으므로 삭제 (메모리 관리)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    /*
     * [이론 설명: 데이터 스토리지 - VAO, VBO]
     * VBO (Vertex Buffer Object): GPU 메모리에 실제 정점 데이터를 담는 공간.
     * VAO (Vertex Array Object): VBO를 어떻게 해석할지(보폭, 타입 등)에 대한 정보를 담는 관리자.
     */

     // --- 4. 데이터 설정 (DSA 스타일: Direct State Access) ---
     // 위치(3) + 색상(3) 정보를 담는 삼각형 데이터
    float vertices[] = {
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, // 상단 (빨강)
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, // 우하단 (초록)
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f  // 좌하단 (파랑)
    };

    unsigned int VAO, VBO;
    // glCreate*: 객체를 생성함과 동시에 상태를 초기화하는 DSA 함수 (glGen*의 개선판)
    glCreateVertexArrays(1, &VAO);
    glCreateBuffers(1, &VBO);

    // glNamedBufferStorage: GPU 메모리를 할당하고 데이터를 복사함. 
    // (이후 데이터 수정을 금지하는 Immutable Storage 방식 - 성능 최적화에 유리)
    glNamedBufferStorage(VBO, sizeof(vertices), vertices, 0);

    // VAO의 0번 바인딩 포인트에 VBO를 연결함 (보폭 6*float 지정)
    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 6 * sizeof(float));

    // [정점 속성 정의: 위치]
    glEnableVertexArrayAttrib(VAO, 0); // 셰이더의 location 0 활성화
    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0); // (인덱스, 크기, 타입, 정규화여부, 상대오프셋)
    glVertexArrayAttribBinding(VAO, 0, 0); // 0번 속성을 0번 바인딩 포인트와 연결

    // [정점 속성 정의: 색상]
    glEnableVertexArrayAttrib(VAO, 1); // 셰이더의 location 1 활성화
    glVertexArrayAttribFormat(VAO, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float)); // 색상은 데이터의 12바이트(3*float) 지점부터 시작
    glVertexArrayAttribBinding(VAO, 1, 0);

    /*
     * [이론 설명: 렌더 루프]
     * 폴링(이벤트 감지) -> 클리어(캔버스 닦기) -> 드로우(그리기) -> 스왑(화면 교체)
     */

     // --- 5. 메인 루프 (프레임 렌더링) ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // 키보드, 마우스 등 입력 이벤트 처리

        // 배경색 설정 및 버퍼 초기화
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 그리기 명령 수행
        glUseProgram(shaderProgram); // 어떤 셰이더 프로그램을 쓸 것인가?
        glBindVertexArray(VAO);      // 어떤 정점 데이터 묶음을 쓸 것인가?
        glDrawArrays(GL_TRIANGLES, 0, 3); // 삼각형 모드로 0번부터 정점 3개를 그려라

        // 더블 버퍼링: 백버퍼에 그린 그림을 프론트버퍼(화면)로 교체
        glfwSwapBuffers(window);
    }

    // --- 6. 자원 해제 ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}