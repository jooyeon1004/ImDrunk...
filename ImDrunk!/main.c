#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h> // printf, scanf, fopen 등 파일 입출력
#include <windows.h> // sleep, SetConsoleCursorPosition 등
#include <stdbool.h> // bool, true, false 명시 가능
#include <conio.h> // kbhit, getchar 등
#include <stdlib.h> // rand, srand, exit 등
#include <time.h> // time 등
#include "player.h"

// --- [R_06 적용] 장애물 종류 세분화 ---
// 오브젝트 구조체 정의
typedef enum {
    TYPE_EXTINGUISHER, // 소화기
    TYPE_TRASHBAG,     // 쓰레기봉투
    TYPE_BOOK,         // 전공책
    TYPE_WATER,        // 물
    TYPE_SOUP          // 해장국
} ObjType;

typedef struct {
    int x; // x좌표
    int y; // y좌표
    ObjType type; // 종류
    bool isActive; // 화면에 현재 존재하는지 여부 (유효한 데이터인지 체크)
} GameObject;

#define MAX_OBJ 10 // 화면에 동시에 존재할 수 있는 최대 오브젝트 수
GameObject objects[MAX_OBJ];

HANDLE hConsole[2]; // 더블 버퍼링을 사용해 화면 깜빡임을 없앰
int screenIndex = 0; // 0번 스크린 인덱스를 사용
int shakeX = 0, shakeY = 0; // 취했을 때의 오프셋 => 초깃값은 0

// 전역 설정 변수
int selectedDifficulty = 0; // 0: 초급, 1: 중급, 2: 고급 (기본값 초급)

void Init(); // 게임 시작 전 초기 세팅을 위한 함수
void ScreenFlipping(); // 스크린 버퍼를 전환하는 함수
void ScreenClear(); // 스크린을 지우는 함수
void ScreenPrint(int x, int y, const char* string); // 지정한 좌표에 글자를 출력하는 함수
void Render(Player p, int difficulty); // 렌더링 함수
void DrawFloor(); // 바닥선 그리는 함수

// --- [R_10 추가] 화면 및 데이터 제어 함수 ---
void ShowStartScreen(); // 시작 화면 및 메뉴 루프
void ShowSettingsScreen(); // 환경설정 (난이도 선택) 화면
void ShowLeaderboardScreen(); // 순위표 출력 화면
void SaveScore(int score); // 게임 종료 시 점수를 txt 파일에 저장하는 함수
bool ShowGameOverScreen(int finalScore); // 종료 화면 출력 및 재시작 여부 반환

int main() {
    Init(); // 초기 세팅

    // 메인 흐름 무한 루프
    while (true) {
        ShowStartScreen(); // 메뉴 선택 로직이 포함된 시작 화면 띄우기

        // --- 데이터 초기화 (본 게임 진입 시 세팅) ---
        Player p1;
        Player_Init(&p1);

        for (int i = 0; i < MAX_OBJ; i++) {
            objects[i].isActive = false;
        }

        bool isGameOver = false;
        int loopCount = 0;

        int framesSinceLastSpawn = 0;
        int consecutiveObstacles = 0;
        int speedDelay = 30;
        int minGap = 20;

        // 사용자가 설정 창에서 고른 난이도를 게임에 바로 적용
        if (selectedDifficulty == 0) {
            speedDelay = 30; minGap = 25;
        }
        else if (selectedDifficulty == 1) {
            speedDelay = 25; minGap = 20;
        }
        else {
            speedDelay = 20; minGap = 15;
        }

        // --- 본 게임 진행 루프 ---
        while (!isGameOver) {
            // A. 입력 처리
            if (_kbhit()) {
                int key = _getch();
                if (key == 32) { Player_Jump(&p1); }
                else if (key == 27) { exit(0); } // ESC 강제 종료
            }

            // B. 로직 업데이트
            Player_Update(&p1);
            p1.score += 1;
            loopCount++;
            framesSinceLastSpawn++;

            if (loopCount % 10 == 0 && p1.drunk_gauge < 100) {
                p1.drunk_gauge += 1;
            }

            // 전체 화면 흔들림 계산
            if (p1.drunk_gauge > 70) {
                shakeX = (rand() % 3) - 1; shakeY = (rand() % 3) - 1;
            }
            else if (p1.drunk_gauge > 30) {
                shakeX = (rand() % 2 == 0) ? 1 : -1; shakeY = 0;
            }
            else {
                shakeX = 0; shakeY = 0;
            }

            // 장애물/아이템 생성 (스폰) 로직
            bool shouldSpawn = false;

            if (selectedDifficulty == 0) { // 초급
                if (framesSinceLastSpawn >= minGap + 10) {
                    shouldSpawn = true;
                }
            }
            else { // 중급 & 고급
                if (framesSinceLastSpawn >= minGap) {
                    if (rand() % 100 < 10) {
                        shouldSpawn = true;
                    }
                    if (framesSinceLastSpawn > minGap + 25) {
                        shouldSpawn = true;
                    }
                }
            }

            // 스폰 결정이 났을 때
            if (shouldSpawn) {
                ObjType nextType;

                if (consecutiveObstacles >= 3) {
                    nextType = TYPE_WATER;
                    consecutiveObstacles = 0;
                }
                else {
                    if (selectedDifficulty == 2 && rand() % 100 < 15) {
                        nextType = (rand() % 100 < 80) ? TYPE_WATER : TYPE_SOUP;
                        consecutiveObstacles = 0;
                    }
                    else {
                        int r = rand() % 3;
                        if (r == 0) nextType = TYPE_EXTINGUISHER;
                        else if (r == 1) nextType = TYPE_TRASHBAG;
                        else nextType = TYPE_BOOK;

                        consecutiveObstacles++;
                    }
                }

                for (int i = 0; i < MAX_OBJ; i++) {
                    if (!objects[i].isActive) {
                        objects[i].isActive = true;
                        objects[i].x = 79;
                        objects[i].y = 24;
                        objects[i].type = nextType;
                        break;
                    }
                }
                framesSinceLastSpawn = 0;
            }

            // 오브젝트 이동 및 충돌 처리
            for (int i = 0; i < MAX_OBJ; i++) {
                if (objects[i].isActive) {
                    objects[i].x--;

                    if (objects[i].x < 0) objects[i].isActive = false;

                    if (objects[i].x == p1.x && (int)p1.y >= objects[i].y - 1) {
                        if (objects[i].type == TYPE_EXTINGUISHER ||
                            objects[i].type == TYPE_TRASHBAG ||
                            objects[i].type == TYPE_BOOK) {
                            isGameOver = true;
                        }
                        else if (objects[i].type == TYPE_WATER) {
                            p1.drunk_gauge -= 30;
                            if (p1.drunk_gauge < 0) p1.drunk_gauge = 0;
                            objects[i].isActive = false;
                        }
                        else if (objects[i].type == TYPE_SOUP) {
                            p1.drunk_gauge = 0;
                            objects[i].isActive = false;
                        }
                    }
                }
            }

            // C. 화면 렌더링
            ScreenClear();
            DrawFloor();
            Render(p1, selectedDifficulty);
            ScreenFlipping();

            Sleep(speedDelay);
        }

        // --- 루프 탈출 (게임 오버 상태) ---
        SaveScore(p1.score); // 텍스트 파일에 최종 점수 기록
        bool wantRestart = ShowGameOverScreen(p1.score);
        if (!wantRestart) {
            break; // 반복문을 완전히 탈출하여 프로그램 종료
        }
    }

    return 0;
}

// --- 화면 흐름 제어 로직 ---

void ShowStartScreen() {
    while (true) { // 사용자가 게임 시작을 누를 때까지 메인 메뉴 무한 반복
        ScreenClear();
        shakeX = 0; shakeY = 0;

        ScreenPrint(25, 8, "==================================");
        ScreenPrint(25, 10, "        주 사 가   달 리 기        ");
        ScreenPrint(25, 12, "==================================");

        ScreenPrint(30, 16, " [1] 게 임  시 작 ");
        ScreenPrint(30, 18, " [2] 환 경  설 정 (난이도)");
        ScreenPrint(30, 20, " [3] 순 위  표 ");
        ScreenPrint(30, 22, " [ESC] 게 임  종 료");

        ScreenFlipping();

        // 사용자 입력 대기
        int key = 0;
        while (true) {
            if (_kbhit()) {
                key = _getch();
                break;
            }
            Sleep(10);
        }

        if (key == '1') return; // 루프 탈출 -> 본 게임 진입
        if (key == '2') ShowSettingsScreen();
        if (key == '3') ShowLeaderboardScreen();
        if (key == 27) exit(0); // ESC 강제 종료
    }
}

void ShowSettingsScreen() {
    while (true) {
        ScreenClear();
        ScreenPrint(28, 8, "=== 환 경  설 정 ===");
        ScreenPrint(25, 12, "원하는 난이도의 숫자를 누르세요.");

        ScreenPrint(30, 15, (selectedDifficulty == 0) ? "-> 1. 초 급 (현재)" : "   1. 초 급");
        ScreenPrint(30, 17, (selectedDifficulty == 1) ? "-> 2. 중 급 (현재)" : "   2. 중 급");
        ScreenPrint(30, 19, (selectedDifficulty == 2) ? "-> 3. 고 급 (현재)" : "   3. 고 급");

        ScreenPrint(26, 24, "[ 아무 키나 누르면 메뉴로 돌아갑니다 ]");
        ScreenFlipping();

        while (!_kbhit()) Sleep(10);
        int key = _getch();

        if (key == '1') selectedDifficulty = 0;
        else if (key == '2') selectedDifficulty = 1;
        else if (key == '3') selectedDifficulty = 2;
        else break; // 1,2,3 외의 키를 누르면 세팅 완료 후 이전 화면으로 복귀
    }
}

void SaveScore(int score) {
    // "rank.txt" 파일을 Append(이어쓰기) 모드로 개방 (통신 포트 개방과 동일)
    FILE* fp = fopen("rank.txt", "a");
    if (fp != NULL) {
        fprintf(fp, "%d\n", score); // 점수를 한 줄씩 기록 (패킷 전송)
        fclose(fp); // 파일 닫기 (메모리 누수 방지용 FIN 신호)
    }
}

void ShowLeaderboardScreen() {
    FILE* fp = fopen("rank.txt", "r"); // 읽기(Read) 모드로 개방
    int scores[100] = { 0 };
    int count = 0;

    // 파일에서 정수를 계속 읽어 배열에 저장
    if (fp != NULL) {
        while (fscanf(fp, "%d", &scores[count]) != EOF && count < 100) {
            count++;
        }
        fclose(fp);
    }

    // 배열을 내림차순(높은 점수가 위로)으로 정렬 (버블 정렬 사용)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (scores[i] < scores[j]) {
                int temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }

    ScreenClear();
    ScreenPrint(30, 8, "=== 순  위  표 (Top 5) ===");

    if (count == 0) {
        ScreenPrint(32, 13, "저장된 기록이 없습니다.");
    }
    else {
        // 최대 5개의 상위 기록만 출력
        for (int i = 0; i < 5 && i < count; i++) {
            char buf[50];
            sprintf(buf, " %d위 : %d 점", i + 1, scores[i]);
            ScreenPrint(32, 12 + (i * 2), buf);
        }
    }

    ScreenPrint(25, 24, "[ 아무 키나 누르면 메뉴로 돌아갑니다 ]");
    ScreenFlipping();

    while (!_kbhit()) Sleep(10);
    _getch(); // 버퍼 비우기
}

bool ShowGameOverScreen(int finalScore) {
    ScreenClear();
    shakeX = 0; shakeY = 0;
    SetConsoleTextAttribute(hConsole[screenIndex], 12);

    ScreenPrint(30, 10, "============================");
    ScreenPrint(32, 12, "  G A M E   O V E R !");

    SetConsoleTextAttribute(hConsole[screenIndex], 7);

    char scoreStr[50];
    sprintf(scoreStr, "  최종 생존 점수 : %d", finalScore);
    ScreenPrint(30, 15, scoreStr);

    ScreenPrint(30, 17, "============================");
    ScreenPrint(22, 22, " Press SPACE to Restart / ESC to Quit ");
    ScreenFlipping();

    while (true) {
        if (_kbhit()) {
            int key = _getch();
            if (key == 32) return true;
            if (key == 27) return false;
        }
        Sleep(10);
    }
}

// 바닥 그리기
void DrawFloor() {
    for (int i = 0; i < 80; i++) {
        ScreenPrint(i, 25, "_");
    }
}

// 렌더링
void Render(Player p, int difficulty) {
    if (p.drunk_gauge > 50) {
        int randomColor = (rand() % 6) + 9;
        SetConsoleTextAttribute(hConsole[screenIndex], randomColor);
    }
    else {
        SetConsoleTextAttribute(hConsole[screenIndex], 7);
    }

    for (int i = 0; i < MAX_OBJ; i++) {
        if (objects[i].isActive) {
            int drawY = objects[i].y;

            if ((objects[i].type == TYPE_EXTINGUISHER ||
                objects[i].type == TYPE_TRASHBAG ||
                objects[i].type == TYPE_BOOK) && p.drunk_gauge > 40) {
                int objShake = (rand() % 3) - 1;
                drawY += objShake;
            }

            const char* symbol = "?";
            switch (objects[i].type) {
            case TYPE_EXTINGUISHER: symbol = "F"; break;
            case TYPE_TRASHBAG:     symbol = "T"; break;
            case TYPE_BOOK:         symbol = "B"; break;
            case TYPE_WATER:        symbol = "W"; break;
            case TYPE_SOUP:         symbol = "S"; break;
            }
            ScreenPrint(objects[i].x, drawY, symbol);
        }
    }

    ScreenPrint(p.x, (int)p.y, "P");

    char scoreTxt[50], gaugeTxt[100], diffTxt[50];
    sprintf(scoreTxt, "Score: %d", p.score);

    // --- [시각적 게이지 처리 로직] ---
    char gaugeBar[15] = "[          ]"; // 10칸짜리 빈 게이지 뼈대
    int fillBlocks = p.drunk_gauge / 10;   // 10단위로 블록 1개씩 계산
    if (fillBlocks > 10) fillBlocks = 10;  // 100 초과 시 방지

    // 채워진 만큼 샵(#) 문자로 덮어쓰기
    for (int i = 0; i < fillBlocks; i++) {
        gaugeBar[i + 1] = '#';
    }
    sprintf(gaugeTxt, "Drunk: %s %d / 100", gaugeBar, p.drunk_gauge);

    const char* diffStr = (difficulty == 0) ? "Beginner" : (difficulty == 1) ? "Intermediate" : "Advanced";
    sprintf(diffTxt, "Level: %s", diffStr);

    ScreenPrint(2, 2, scoreTxt);
    ScreenPrint(2, 3, gaugeTxt); // 텍스트 게이지 대신 시각적 게이지 출력
    ScreenPrint(2, 4, diffTxt);

    SetConsoleTextAttribute(hConsole[screenIndex], 7);
}

// --- 콘솔 제어 함수들 ---
void Init() {
    CONSOLE_CURSOR_INFO cursorInfo = { 0, };
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = FALSE;

    hConsole[0] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    hConsole[1] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    SetConsoleCursorInfo(hConsole[0], &cursorInfo);
    SetConsoleCursorInfo(hConsole[1], &cursorInfo);

    srand((unsigned int)time(NULL));
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

void ScreenPrint(int x, int y, const char* string) {
    DWORD dw;
    COORD cursorPosition = { x + shakeX, y + shakeY };

    if (cursorPosition.X < 0) cursorPosition.X = 0;
    if (cursorPosition.Y < 0) cursorPosition.Y = 0;

    SetConsoleCursorPosition(hConsole[screenIndex], cursorPosition);
    WriteFile(hConsole[screenIndex], string, strlen(string), &dw, NULL);
}