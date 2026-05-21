#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h> // printf, scanf 등
#include <windows.h> // sleep, SetConsoleCursorPosition, system("cls") 등
#include <stdbool.h> // bool, true, false 명시 가능
#include <conio.h> // kbhit, getchar 등
#include <stdlib.h> // rand, srand, exit 등
#include <time.h> // time 등
#include "player.h"

// 오브젝트 구조체 정의
typedef enum { TYPE_OBSTACLE, TYPE_WATER, TYPE_SOUP } ObjType; //0=장애물, 1=물, 2=해장국

typedef struct {
    int x; //x좌표
    int y; //y좌표
    ObjType type; //종류
    bool isActive; // 화면에 현재 존재하는지 여부 (유효한 데이터인지 체크)
} GameObject;

#define MAX_OBJ 5 // 화면에 동시에 존재할 수 있는 최대 오브젝트 수
GameObject objects[MAX_OBJ];

HANDLE hConsole[2]; // 더블 버퍼링을 사용해 화면 깜빡임을 없앰
int screenIndex = 0; //0번 스크린 인덱스를 사용
int shakeX = 0, shakeY = 0; // 취했을 때의 오프셋 => 초깃값은 0

void Init(); // 게임 시작 전 초기 세팅을 위한 함수
void ScreenFlipping(); // 스크린 버퍼를 전환하는 함수
void ScreenClear(); // 스크린을 지우는 함수
void ScreenPrint(int x, int y, const char* string); // 지정한 x,y 좌표에 글자를 출력하는 함수 + 화면 흔들기 기능도 담당
void Render(Player p); // 플레이어 정보(p)를 받아서 화면 전체를 렌더링하는 함수
void DrawFloor(); // 바닥선 (___) 그리는 함수

int main() {
    Init(); // Init 호출(밑에 동작 명시)
    Player p1; // 플레이어 할당
    Player_Init(&p1); // player.c 속 설정값을 p1에게 넘겨줌

    // 오브젝트 배열 초기화
    for (int i = 0; i < MAX_OBJ; i++) {
        objects[i].isActive = false; // 초기화를 위해 MAX_OBJ번째 objects 배열까지 순서대로 isActive 값을 false로 설정함
    }

    bool isGameOver = false; // 게임 오버가 아닌 상태로 설정
    int loopCount = 0; // while문이 돈 루프 수로 게이지 및 장애물 생성을 결정하므로, loopcount 변수를 생성하고 0으로 초기화

    while (!isGameOver) { // 게임 오버가 아닐 때까지
        // A. 입력 처리
        if (_kbhit()) { // 키를 눌렀으면
            int key = _getch(); // 입력된 키가 무엇인지 확인하기 위해 키값을 getch를 이용해 key 변수에 넣음
            if (key == 32) { Player_Jump(&p1); } // 키값이 32(=스페이스바)라면 점프
            else if (key == 27) { isGameOver = true; } // 키값이 27(=esc)이라면 게임 오버 처리(게임 종료)
        }

        // B. 로직 업데이트
        Player_Update(&p1);
        p1.score += 1; // 점수를 1씩 늘리기
        loopCount++; // 루프 돈 횟수를 하나 늘리기

        if (loopCount % 10 == 0 && p1.drunk_gauge < 100) { // 루프가 10번 돌았고(10의 배수이고) 취함 게이지가 100보다 작으면(게임 오버 조건이 아니면)
            p1.drunk_gauge += 1; // 취함 게이지 1 늘리기
        }

        // 전체 화면 흔들림 계산
        if (p1.drunk_gauge > 70) { // 취함 게이지가 70을 넘으면
            shakeX = (rand() % 3) - 1; shakeY = (rand() % 3) - 1; // 좌우 상하로 -1,0,1씩 랜덤으로 흔들림
        }
        else if (p1.drunk_gauge > 30) { // 취함 게이지가 30을 넘으면
            shakeX = (rand() % 2 == 0) ? 1 : -1; shakeY = 0; // 좌우로만 1,-1씩 랜덤으로 흔들림
        }
        else { // 그 외(취함 게이지가 30 이하)
            shakeX = 0; shakeY = 0; // 안 흔들림
        }

        // 오브젝트 스폰 로직
        if (loopCount % 40 == 0) { // 40프레임마다 오브젝트 생성 시도
            for (int i = 0; i < MAX_OBJ; i++) { // 총 개수가 MAX_OBJ보다 작을 동안만
                if (!objects[i].isActive) { //i번째 오브젝트가 isActive 상태가 아니라면
                    objects[i].isActive = true; // 오브젝트 소환
                    objects[i].x = 79; // 화면 맨 오른쪽 끝에서 시작
                    objects[i].y = 24; // 바닥 높이

                    // 종류를 랜덤하게 결정 (장애물 85%, 물 12%, 해장국 3%)
                    // 난수 범위를 0~99로 확장해서 세밀한 확률 적용
                    int r = rand() % 100;
                    if (r < 85) {
                        objects[i].type = TYPE_OBSTACLE; // 85% 확률로 장애물
                    }
                    else if (r < 97) {
                        objects[i].type = TYPE_WATER;    // 12% 확률로 물
                    }
                    else {
                        objects[i].type = TYPE_SOUP;     // 3% 확률로 해장국 (아주 희귀함)
                    }
                    break;
                }
            }
        }

        // 오브젝트 이동 및 충돌 처리
        for (int i = 0; i < MAX_OBJ; i++) {
            if (objects[i].isActive) { //i번째 오브젝트가 isActive 상태라면
                objects[i].x--; // 왼쪽으로 다가옴(x좌표 하나씩 줄임)

                // 화면 밖으로 나가면 비활성화
                if (objects[i].x < 0) objects[i].isActive = false;

                // 충돌 감지 (플레이어 X좌표와 같고, 점프 중이 아니라서 Y좌표가 겹칠 때)
                if (objects[i].x == p1.x && (int)p1.y >= objects[i].y - 1) {
                    if (objects[i].type == TYPE_OBSTACLE) { // 오브젝트의 종류가 장애물이라면
                        isGameOver = true; // 게임 오버
                    }
                    else if (objects[i].type == TYPE_WATER) { // 오브젝트의 종류가 물이라면
                        // [R_05 구현] 물: 게이지 30 감소
                        p1.drunk_gauge -= 30;
                        if (p1.drunk_gauge < 0) p1.drunk_gauge = 0; // 이미 게이지가 0이라면 그냥 0으로 유지
                        objects[i].isActive = false; // 먹었으니 사라짐
                    }
                    else if (objects[i].type == TYPE_SOUP) { // 오브젝트의 종류가 해장국이라면
                        // [R_05 구현] 해장국: 게이지 0으로 완전 초기화
                        p1.drunk_gauge = 0;
                        objects[i].isActive = false; // 먹었으니 사라짐
                    }
                }
            }
        }

        // C. 화면 렌더링
        ScreenClear(); // 화면 지우기
        DrawFloor(); // 바닥 그리기
        Render(p1); // 캐릭터/장애물 렌더링하기
        ScreenFlipping(); // 화면 전환하기

        Sleep(30); // 0.03초 동안 쉬기
    }

    // 게임 오버 시 텍스트 출력
    ScreenClear(); // 화면 지우기
    ScreenPrint(35, 15, "G A M E   O V E R !"); // 일정 좌표에 game over 띄우기
    ScreenFlipping(); // 화면 전환하기 
    Sleep(2000); // 2초 대기 후 종료

    return 0;
}

// 바닥 그리기
void DrawFloor() {
    for (int i = 0; i < 80; i++) { // i값(=x좌표)가 0부터 79일 동안
        ScreenPrint(i, 25, "_"); // y좌표 25에 _ 그리기 (0~79에 다 그리면 바닥처럼 보임)
    }
}

// 렌더링
void Render(Player p) {
    if (p.drunk_gauge > 50) { // 취함 게이지가 50이 넘으면
        int randomColor = (rand() % 6) + 9; // 컬러에 해당하는 색상을 난수로 결정
        SetConsoleTextAttribute(hConsole[screenIndex], randomColor); // 난수에 따른 색상으로 변경
    }
    else {
        SetConsoleTextAttribute(hConsole[screenIndex], 7); // 50을 안 넘었다면 기본으로 유지
    }

    // 오브젝트 렌더링 (R_04 착시 현상 적용)
    for (int i = 0; i < MAX_OBJ; i++) { // MAX_OBJ 범위 내에서
        if (objects[i].isActive) { // 오브젝트가 active 상태라면
            int drawY = objects[i].y; // 오브젝트의 y좌표 값을 drawY 변수에 저장

            // [R_04 구현] 취함 게이지가 40 이상일 때 장애물 위치 흔들림 적용
            if (objects[i].type == TYPE_OBSTACLE && p.drunk_gauge > 40) { // 오브젝트 종류가 장애물이고 취함 게이지가 40 이상이라면
                int objShake = (rand() % 3) - 1; // Y좌표에 -1, 0, 1 난수 발생
                drawY += objShake; // 발생된 난수만큼 drawY 값에 더해줌
            }

            // 종류별로 다른 문자 출력 (차후 아스키 아트로 변경 가능)
            const char* symbol = "X"; // 기본값(=장애물)
            if (objects[i].type == TYPE_WATER) symbol = "W"; // 오브젝트 종류가 물일 경우
            else if (objects[i].type == TYPE_SOUP) symbol = "S"; // 오브젝트 종류가 해장국일 경우

            ScreenPrint(objects[i].x, drawY, symbol); // 렌더링된 오브젝트를 화면에 표출함
        }
    }

    // 캐릭터 출력
    ScreenPrint(p.x, (int)p.y, "P"); // P 모양으로 캐릭터 출력

    // UI 출력
    char scoreTxt[50], gaugeTxt[50];
    sprintf(scoreTxt, "Score: %d", p.score); // sprintf를 사용해 포맷팅
    sprintf(gaugeTxt, "Drunk Gauge: %d / 100", p.drunk_gauge); // sprintf를 사용해 포맷팅
    ScreenPrint(2, 2, scoreTxt); // x좌표 2, y좌표 2에 스코어 배치
    ScreenPrint(2, 3, gaugeTxt);
    ScreenPrint(2, 5, "Press Space to Jump / ESC to Exit");

    SetConsoleTextAttribute(hConsole[screenIndex], 7); // 콘솔 글자 색상을 7(흰색)으로 설정
}


// 콘솔 제어 함수들

void Init() {
    CONSOLE_CURSOR_INFO cursorInfo = { 0, }; // 구조체 인스턴스 생성
    cursorInfo.dwSize = 1; // 커서 두께를 1로 설정(최소화)
    cursorInfo.bVisible = FALSE; // 깜빡거리는 커서를 안 보이게 설정

    hConsole[0] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL); // 핸들 0 생성, 읽고 쓰기 가능
    hConsole[1] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL); // 핸들 1 생성, 읽고 쓰기 가능

    SetConsoleCursorInfo(hConsole[0], &cursorInfo);
    SetConsoleCursorInfo(hConsole[1], &cursorInfo);

    // 랜덤 함수 시드 초기화 (시간 기준)
    srand((unsigned int)time(NULL)); // 겹치지 않는 난수를 생성하도록 함
}

void ScreenFlipping() {
    SetConsoleActiveScreenBuffer(hConsole[screenIndex]);
    screenIndex = !screenIndex;
}

void ScreenClear() {
    COORD co = { 0, 0 };
    DWORD dw;
    FillConsoleOutputCharacter(hConsole[screenIndex], ' ', 120 * 30, co, &dw);
}

// 기존 ScreenPrint 함수에 흔들림 오프셋 적용
void ScreenPrint(int x, int y, const char* string) {
    DWORD dw;
    // 원래 좌표에 노이즈(shakeX, shakeY)를 더해서 출력 좌표 결정
    COORD cursorPosition = { x + shakeX, y + shakeY };

    // 콘솔 창 밖으로 좌표가 넘어가는 것을 방지 (에러 방지)
    if (cursorPosition.X < 0) cursorPosition.X = 0;
    if (cursorPosition.Y < 0) cursorPosition.Y = 0;

    SetConsoleCursorPosition(hConsole[screenIndex], cursorPosition);
    WriteFile(hConsole[screenIndex], string, strlen(string), &dw, NULL);
}