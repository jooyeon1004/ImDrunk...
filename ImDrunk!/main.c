#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h> 
#include <windows.h> // WinAPI 사용 (콘솔 제어, 스레드, 시스템 입력 등)
#include <stdbool.h> // bool 데이터 타입 사용
#include <conio.h>   // _kbhit(), _getch() 등 콘솔 입력 함수
#include <stdlib.h>  // rand(), srand() 등 함수
#include <string.h>  // strcpy(), strlen() 등 문자열 제어
#include <math.h>    // sin() 함수 (메인 화면 UI 애니메이션 용도)
#include <time.h>    // 난수 시드 생성을 위한 시간 함수

// 윈도우 멀티미디어 API 연동
#pragma comment(lib, "winmm.lib")
#include <mmsystem.h>
#include "player.h"

// 열거형 및 구조체 정의

// 게임 내 등장하는 장애물 및 아이템 종류 정의
typedef enum {
    TYPE_EXTINGUISHER, // 0: 소화기 (장애물)
    TYPE_TRASHBAG,     // 1: 쓰레기봉투 (장애물)
    TYPE_BOOK,         // 2: 전공책 (장애물)
    TYPE_WATER,        // 3: 물 (아이템 - 취함 게이지 30 감소)
    TYPE_SOUP          // 4: 해장국 (아이템 - 취함 게이지 0으로 초기화)
} ObjType;

// 오브젝트 상태 정보를 담는 구조체
typedef struct {
    int x;          // 현재 x 좌표
    int y;          // 현재 y 좌표
    ObjType type;   // 오브젝트의 종류
    bool isActive;  // 현재 화면에 활성화되어 이동 중인지 여부
} GameObject;

#define MAX_OBJ 10  // 한 화면에 동시에 존재할 수 있는 최대 오브젝트 개수
GameObject objects[MAX_OBJ]; // 오브젝트 객체 풀 배열

// 전역 변수

// 깜빡임 방지용 더블 버퍼링
HANDLE hConsole[2];
int screenIndex = 0; // 현재 활성화된 버퍼의 인덱스 (0 또는 1)

// 취함 게이지가 높을 때 흔들림을 위한 변수
int shakeX = 0, shakeY = 0;
// 사용자가 선택한 게임 난이도 (0: 간술, 1: 회식, 2: MT, 3: 만취)
int selectedDifficulty = 0;


// 오디오 시스템 제어 함수

// 1. BGM 재생 (mciSendString 사용)
// mp3 형식의 파일을 백그라운드에서 무한 반복 재생
void PlayBGM(const char* filename) {
    char cmd[256];
    mciSendStringA("close bgm", NULL, 0, NULL); // 기존에 재생 중이던 BGM이 있다면 닫기 (메모리 누수 방지)
    sprintf(cmd, "open \"%s\" type mpegvideo alias bgm", filename); // 파일을 열고 'bgm'이라는 별명 부여
    mciSendStringA(cmd, NULL, 0, NULL);
    mciSendStringA("play bgm repeat", NULL, 0, NULL); // repeat 옵션을 주어 백그라운드 재생
}

// BGM 정지 함수 (게임 종료 시 사용)
void StopBGM() {
    mciSendStringA("stop bgm", NULL, 0, NULL);
}

// 2. 점프 시 비프음 스레드
// 메인 스레드를 멈추지 않기 위해 별도의 스레드에서 Beep API 호출
DWORD WINAPI JumpBeepThread(LPVOID p) {
    Beep(523, 60); // 523Hz 주파수 소리를 60ms 동안 재생
    return 0;
}

// 3. 효과음 재생 (PlaySoundA 사용)
void PlaySFX(const char* filename) {
    // SND_FILENAME: 파일 경로로 재생 / SND_ASYNC: 비동기(렉 방지) / SND_NODEFAULT: 파일 없어도 에러음 무시
    PlaySoundA(filename, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

// 함수 원형 선언
void Init();
void ScreenFlipping();
void ScreenClear();
void ScreenPrint(int x, int y, const char* string);
void Render(Player p, int difficulty);
void DrawFloor(int drunk_gauge);
void ShowStartScreen();
void ShowSettingsScreen();
void ShowLeaderboardScreen();
void SaveScore(int score, int difficulty);
int ShowGameOverScreen(int finalScore);


// 메인 함수
int main() {
    SetConsoleOutputCP(65001);
    Init(); // 콘솔 버퍼 생성, 사이즈 고정, QuickEdit 모드 해제 등 시스템 초기화
    int nextAction = 2; // 분기 제어 변수 (1: 바로 게임 시작, 2: 메인 화면 진입)

    // 게임 루프 시작 전 BGM 재생
    PlayBGM("bgm.mp3");

    // 무한 루프 (사용자가 ESC를 누를 때까지 반복)
    while (true) {
        Player p1;
        int i, r;
        bool isGameOver;
        int loopCount;            // 현재 프레임 카운트 (취함 게이지 증가용)
        int framesSinceLastSpawn; // 마지막 오브젝트 생성 이후 경과 프레임
        int baseSpeedDelay;       // 난이도별 기본 프레임 딜레이(속도)
        int minGap;               // 장애물 간의 최소 스폰 간격 (프레임 단위)
        int currentSpeed;         // 점수에 따라 점점 빨라지는 스피드
        bool shouldSpawn;         // 이번 프레임에 오브젝트를 스폰할지 여부
        ObjType nextType;         // 다음에 스폰할 오브젝트 종류

        // 2번이 선택된 경우 메인 화면 출력
        if (nextAction == 2) {
            ShowStartScreen();
        }

        // 플레이어 위치 및 초기 상태 세팅 (player.h)
        Player_Init(&p1);

        // 오브젝트 풀 초기화
        for (i = 0; i < MAX_OBJ; i++) {
            objects[i].isActive = false;
        }

        // 인게임 변수 초기화
        isGameOver = false;
        loopCount = 0;
        framesSinceLastSpawn = 0;
        bool isSpacePressed = false; // 점프 키 꾹 누름(연속 점프) 방지

        // 난이도별 밸런스
        if (selectedDifficulty == 0) { // 간술 (쉬움)
            baseSpeedDelay = 30; minGap = 25;
        }
        else if (selectedDifficulty == 1) { // 회식 (보통)
            baseSpeedDelay = 25; minGap = 20;
        }
        else if (selectedDifficulty == 2) { // MT (어려움)
            baseSpeedDelay = 20; minGap = 15;
        }
        else if (selectedDifficulty == 3) { // 만취 (매우 어려움)
            baseSpeedDelay = 15; minGap = 15;
        }

        // [인게임 메인 루프] 게임 오버가 될 때까지 1프레임 단위로 계속됨
        while (!isGameOver) {

            // 1. 입력 처리
            // GetAsyncKeyState: 콘솔 버퍼를 거치지 않고 키보드 전기 신호를 직접 캐치함
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                if (!isSpacePressed) { // 이전 프레임에 안 눌려있었을 때만 1회 점프 허용
                    Player_Jump(&p1);
                    // 점프 비프음 재생
                    HANDLE hThread = CreateThread(NULL, 0, JumpBeepThread, NULL, 0, NULL);
                    if (hThread) CloseHandle(hThread); // 핸들을 닫아 스레드 메모리 누수 방지
                    isSpacePressed = true;
                }
            }
            else {
                isSpacePressed = false; // 키를 떼면 플래그 해제
            }

            // ESC 키 감지 시 즉시 프로그램 강제 종료
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                exit(0);
            }

            // 2. 플레이어 및 상태 업데이트
            Player_Update(&p1); // 플레이어의 중력 적용 및 y좌표 갱신
            p1.score += 1;      // 생존 프레임마다 점수 1씩 증가
            loopCount++;
            framesSinceLastSpawn++;

            // 3. 취함 게이지 제어
            if (selectedDifficulty == 3) {
                p1.drunk_gauge = 100; // 만취 난이도는 게이지 항상 100 고정
            }
            else if (loopCount % 10 == 0 && p1.drunk_gauge < 100) {
                p1.drunk_gauge += 1;  // 10프레임마다 취함 게이지 1씩 증가
            }

            // 취함 게이지에 따른 화면 흔들림 효과
            if (p1.drunk_gauge > 70) { // 게이지가 70이 넘으면
                shakeX = (rand() % 3) - 1; shakeY = (rand() % 3) - 1; // x,y축 심하게 진동 (-1 ~ 1)
            }
            else if (p1.drunk_gauge > 30) { // 30이 넘으면
                shakeX = (rand() % 2 == 0) ? 1 : -1; shakeY = 0; // x축만 가볍게 흔들림
            }
            else {
                shakeX = 0; shakeY = 0; // 정상 상태
            }

            // 4. 오브젝트 스폰
            shouldSpawn = false;
            if (selectedDifficulty == 0) {
                // 간술 난이도: 최소 간격보다 10프레임 더 여유를 두고 스폰
                if (framesSinceLastSpawn >= minGap + 10) shouldSpawn = true;
            }
            else {
                // 그 외 난이도: 최소 간격이 지나면 10% 확률로 랜덤 스폰되거나, 최대 간격 도달 시 확정 스폰
                if (framesSinceLastSpawn >= minGap) {
                    if (rand() % 100 < 10) shouldSpawn = true;
                    if (framesSinceLastSpawn > minGap + 25) shouldSpawn = true;
                }
            }

            // 스폰이 결정되었다면 어떤 오브젝트를 띄울지 결정
            if (shouldSpawn) {
                if (selectedDifficulty == 3) {
                    // 만취 난이도: 아이템 안 나옴 (오직 장애물만 나옴)
                    r = rand() % 3;
                    if (r == 0) nextType = TYPE_EXTINGUISHER;
                    else if (r == 1) nextType = TYPE_TRASHBAG;
                    else nextType = TYPE_BOOK;
                }
                else {
                    // 난이도별 아이템 등장 확률 계산
                    int itemChance = 0;
                    if (selectedDifficulty == 0) itemChance = 20; // 간술 20%
                    else if (selectedDifficulty == 1) itemChance = 12; // 회식 12%
                    else if (selectedDifficulty == 2) itemChance = 7;  // MT 7%

                    if (rand() % 100 < itemChance) {
                        // 아이템이 뜨기로 결정되면: 80% 확률로 물, 20% 확률로 해장국
                        nextType = (rand() % 100 < 80) ? TYPE_WATER : TYPE_SOUP;
                    }
                    else {
                        // 장애물이 뜨기로 결정되면 3가지 중 하나 랜덤
                        r = rand() % 3;
                        if (r == 0) nextType = TYPE_EXTINGUISHER;
                        else if (r == 1) nextType = TYPE_TRASHBAG;
                        else nextType = TYPE_BOOK;
                    }
                }

                // 풀을 돌며 빈 자리(비활성 상태)에 새 오브젝트 세팅
                for (i = 0; i < MAX_OBJ; i++) {
                    if (!objects[i].isActive) {
                        objects[i].isActive = true;
                        objects[i].x = 79;          // 화면 우측 끝에서 시작
                        objects[i].type = nextType; // 결정된 타입 할당

                        // 취함 게이지가 80 이상이면 바닥이 올라가므로(T자 바닥), 오브젝트의 y좌표도 위쪽으로 매핑
                        if (p1.drunk_gauge >= 80) {
                            objects[i].y = 10;
                        }
                        else {
                            objects[i].y = 24; // 평상시 바닥 높이
                        }
                        break; // 하나 스폰했으므로 루프 탈출
                    }
                }
                framesSinceLastSpawn = 0; // 스폰 경과 프레임 초기화
            }

            // 5. 모든 오브젝트 이동 및 플레이어와의 충돌 판정
            for (i = 0; i < MAX_OBJ; i++) {
                if (objects[i].isActive) {
                    objects[i].x--; // 왼쪽으로 1칸 이동

                    // 화면 왼쪽 밖으로 나가면 객체 비활성화
                    if (objects[i].x < 0) objects[i].isActive = false;

                    // 충돌 판정: x좌표가 일치하고 y좌표 차이가 1 이하일 때
                    if (objects[i].x == p1.x && abs((int)p1.y - objects[i].y) <= 1) {
                        // 장애물 충돌 시 게임 오버
                        if (objects[i].type == TYPE_EXTINGUISHER ||
                            objects[i].type == TYPE_TRASHBAG ||
                            objects[i].type == TYPE_BOOK) {
                            isGameOver = true;
                            PlaySFX("gameover.wav"); // 게임오버 사운드 재생
                        }
                        // 물 충돌 시 게이지 30 감소
                        else if (objects[i].type == TYPE_WATER) {
                            p1.drunk_gauge -= 30;
                            if (p1.drunk_gauge < 0) p1.drunk_gauge = 0; // 0 밑으로 안 떨어지게 함
                            objects[i].isActive = false; // 먹었으니 화면에서 제거
                            PlaySFX("item.wav");
                        }
                        // 해장국 충돌 시 게이지 0으로 리셋
                        else if (objects[i].type == TYPE_SOUP) {
                            p1.drunk_gauge = 0;
                            objects[i].isActive = false;
                            PlaySFX("item.wav");
                        }
                    }
                }
            }

            // 6. 화면 렌더링
            ScreenClear();                   // 이전 프레임 잔상 지우기
            DrawFloor(p1.drunk_gauge);       // 상태에 맞는 바닥 그리기
            Render(p1, selectedDifficulty);  // 플레이어, 장애물, UI 등을 백버퍼에 그리기
            ScreenFlipping();                // 백버퍼와 프론트버퍼를 교체하여 화면 출력 (깜빡임 방지)

            // 7. 게임 속도 제어
            // 점수가 오를수록 (=시간이 지날수록) currentSpeed 딜레이를 깎아서 게임 속도를 가속함
            currentSpeed = baseSpeedDelay - (p1.score / 600);
            if (currentSpeed < 10) currentSpeed = 10; // 최고 속도 한계치 지정 (10ms)
            Sleep(currentSpeed); // OS 스레드 휴식 (프레임 유지)
        } // 메인 루프 종료

        // 게임 오버 후 남아있는 키보드 입력 버퍼를 비워줌 (메뉴 화면으로의 오작동 넘어감 방지)
        while (_kbhit()) _getch();

        // 게임 결과 점수를 txt 파일에 저장
        SaveScore(p1.score, selectedDifficulty);

        // 게임 오버 UI 호출 및 사용자의 다음 행동 선택 분기
        int choice = ShowGameOverScreen(p1.score);
        if (choice == 1) nextAction = 1;      // 바로 재시작
        else if (choice == 2) nextAction = 2; // 메인 화면으로 돌아가기
        else break;                           // ESC 종료 시 메인 while문 탈출
    }

    // 프로그램 종료 전 메모리 및 장치 클린업
    StopBGM();
    mciSendStringA("close all", NULL, 0, NULL);
    return 0; // 프로세스 정상 종료
}


// 화면 제어 및 서브 기능 함수들


// 게임 메인 타이틀 화면
void ShowStartScreen() {
    int key;
    int menuFrame = 0; // 사인파 애니메이션 등을 위한 프레임 카운터

    while (true) {
        ScreenClear();
        shakeX = 0; shakeY = 0; // 메인화면은 흔들림 없음

        // 상하단 테두리 선 그리기
        SetConsoleTextAttribute(hConsole[screenIndex], 8); // 회색
        for (int x = 10; x < 70; x++) {
            ScreenPrint(x, 4, "▄");
            ScreenPrint(x, 26, "▀");
        }
        for (int y = 5; y < 26; y++) {
            ScreenPrint(10, y, "█");
            ScreenPrint(69, y, "█");
        }

        // 배경 기포 상승 애니메이션
        SetConsoleTextAttribute(hConsole[screenIndex], 9); // 파란색
        for (int b = 0; b < 5; b++) {
            // sin 함수를 활용해 기포가 좌우로 일렁이며 위로 올라가게 구현
            int bx = 15 + (b * 12) + (int)(sin((menuFrame + b * 10) * 0.1f) * 2);
            int by = 24 - ((menuFrame + b * 15) % 20);
            if (by > 5 && bx > 10 && bx < 68) {
                ScreenPrint(bx, by, (b % 2 == 0) ? "o" : "O"); // 크기가 교차되는 기포
            }
        }

        // 타이틀 로고 출력 (깜빡임 효과 포함)
        if (menuFrame % 30 < 15) SetConsoleTextAttribute(hConsole[screenIndex], 14); // 노란색
        else SetConsoleTextAttribute(hConsole[screenIndex], 11);                     // 민트색

        ScreenPrint(22, 7, "█▀▀▀▀▀▀▀▀▀▀▀▀▀█▀▀▀▀▀█▀▀▀▀▀▀▀▀▀▀▀▀█");
        ScreenPrint(22, 8, "█   ▄█▀▀▀█▄   █     █   ▄████▄   █");
        ScreenPrint(22, 9, "█  ▐█▌   ▐█▌  █     █  ▐█▌  ▐█▌  █");
        ScreenPrint(22, 10, "█   ▀█▄▄▄█▀   ▀▄▄▄▄▀▀   ▀████▀   █");
        ScreenPrint(22, 11, "█▄▄▄▄▄▄▄▄▄▄▄▄▄█▄▄▄▄▄█▄▄▄▄▄▄▄▄▄▄▄▄█");

        SetConsoleTextAttribute(hConsole[screenIndex], 15); // 흰색
        ScreenPrint(26, 13, "주  사  가    달  리  기 !");

        // 메뉴 선택지 출력
        SetConsoleTextAttribute(hConsole[screenIndex], 7); // 밝은 회색
        ScreenPrint(24, 16, " ▐█▀▄  [1] 게 임  시 작 ");
        ScreenPrint(24, 18, " ▐█▀▄  [2] 환 경  설 정 (난이도) ");
        ScreenPrint(24, 20, " ▐█▀▄  [3] 순 위  표 ");
        ScreenPrint(24, 22, " ▐█▀▄  [ESC] 게 임  종 료 ");

        // 프롬프트 안내 텍스트 점멸 애니메이션
        if (menuFrame % 20 < 10) {
            SetConsoleTextAttribute(hConsole[screenIndex], 10); // 초록색
            ScreenPrint(24, 24, "▶ Press Menu Number to Select ◀");
        }

        ScreenFlipping();
        menuFrame++;

        // 사용자 입력 처리
        if (_kbhit()) {
            key = _getch();
            if (key == '1') return; // 게임 시작 (메인 루프 진입)
            if (key == '2') ShowSettingsScreen();
            if (key == '3') ShowLeaderboardScreen();
            if (key == 27) exit(0); // ESC 
        }
        Sleep(30);
    }
}

// 난이도 설정 화면 UI 로직
void ShowSettingsScreen() {
    int key;
    int animFrame = 0;
    while (true) {
        ScreenClear();

        SetConsoleTextAttribute(hConsole[screenIndex], 11); // 민트색 테두리
        for (int x = 12; x < 68; x++) {
            ScreenPrint(x, 5, "▄"); ScreenPrint(x, 24, "▀");
        }
        for (int y = 6; y < 24; y++) {
            ScreenPrint(12, y, "█"); ScreenPrint(67, y, "█");
        }

        SetConsoleTextAttribute(hConsole[screenIndex], 15);
        ScreenPrint(28, 8, "▄████▄  환경 설정  ▄████▄");

        SetConsoleTextAttribute(hConsole[screenIndex], 7);
        ScreenPrint(22, 11, "원하는 난이도의 숫자를 누르세요.");

        // 현재 선택된 난이도에 따라 포인터 및 색상 강조
        if (selectedDifficulty == 0) {
            SetConsoleTextAttribute(hConsole[screenIndex], 10);
            ScreenPrint(24, 14, "► ▐█▀  1. 간 술 ");
        }
        else {
            SetConsoleTextAttribute(hConsole[screenIndex], 7);
            ScreenPrint(24, 14, "   ▐▄▄  1. 간 술 ");
        }

        if (selectedDifficulty == 1) {
            SetConsoleTextAttribute(hConsole[screenIndex], 14);
            ScreenPrint(24, 16, "► ▐█▀  2. 회 식 ");
        }
        else {
            SetConsoleTextAttribute(hConsole[screenIndex], 7);
            ScreenPrint(24, 16, "   ▐▄▄  2. 회 식 ");
        }

        if (selectedDifficulty == 2) {
            SetConsoleTextAttribute(hConsole[screenIndex], 12);
            ScreenPrint(24, 18, "► ▐█▀  3. M  T  ");
        }
        else {
            SetConsoleTextAttribute(hConsole[screenIndex], 7);
            ScreenPrint(24, 18, "   ▐▄▄  3. M  T  ");
        }

        if (selectedDifficulty == 3) {
            // 만취 난이도 강조를 위한 점멸 효과
            SetConsoleTextAttribute(hConsole[screenIndex], (animFrame % 10 < 5) ? 13 : 12);
            ScreenPrint(24, 20, "► ▐█▀  !! 만 취 !!");
        }
        else {
            SetConsoleTextAttribute(hConsole[screenIndex], 8);
            ScreenPrint(24, 20, "   ▐▄▄  4. 0_0");
        }

        SetConsoleTextAttribute(hConsole[screenIndex], 8);
        ScreenPrint(22, 22, "--------------------------------------");

        if (animFrame % 20 < 10) {
            SetConsoleTextAttribute(hConsole[screenIndex], 15);
            ScreenPrint(21, 23, "[ 아무 키나 누르면 메뉴로 돌아갑니다 ]");
        }

        ScreenFlipping();
        animFrame++;

        // 난이도 변경 및 화면 탈출
        if (_kbhit()) {
            key = _getch();
            if (key == '1') selectedDifficulty = 0;
            else if (key == '2') selectedDifficulty = 1;
            else if (key == '3') selectedDifficulty = 2;
            else if (key == '4') selectedDifficulty = 3;
            else break; // 1~4번 외 아무 키나 누르면 탈출 (메뉴로 복귀)
        }
        Sleep(30);
    }
}

// 점수를 파일에 저장
void SaveScore(int score, int difficulty) {
    FILE* fp;
    fp = fopen("rank.txt", "a"); // Append 모드: 파일 끝에 내용 이어붙이기
    if (fp != NULL) {
        fprintf(fp, "%d %d\n", score, difficulty); // "점수 난이도" 포맷으로 저장
        fclose(fp); // 파일 핸들 닫기
    }
}

// 순위표 화면
void ShowLeaderboardScreen() {
    FILE* fp = fopen("rank.txt", "r"); // 읽기 모드로 파일 오픈
    int scores[100] = { 0 }; // 최대 100개의 점수를 담을 배열
    int diffs[100] = { 0 };  // 각 점수의 난이도를 담을 배열
    int count = 0;
    int i, j, tempScore, tempDiff;
    char buf[100];
    char diffStr[50];
    int animFrame = 0;

    // 파일에서 현재 선택된 난이도의 데이터만 필터링하여 읽어오기
    if (fp != NULL) {
        int tempS, tempD;
        while (fscanf(fp, "%d %d", &tempS, &tempD) != EOF && count < 100) {
            if (tempD == selectedDifficulty) {
                scores[count] = tempS;
                diffs[count] = tempD;
                count++;
            }
        }
        fclose(fp);
    }

    // 버블 정렬을 사용하여 점수를 내림차순으로 정렬
    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (scores[i] < scores[j]) {
                // 점수 교환
                tempScore = scores[i];
                scores[i] = scores[j];
                scores[j] = tempScore;

                // 난이도 배열도 같이 교환
                tempDiff = diffs[i];
                diffs[i] = diffs[j];
                diffs[j] = tempDiff;
            }
        }
    }

    while (true) {
        ScreenClear();

        SetConsoleTextAttribute(hConsole[screenIndex], 14);
        for (int x = 10; x < 70; x++) {
            ScreenPrint(x, 3, "▄"); ScreenPrint(x, 26, "▀");
        }
        for (int y = 4; y < 26; y++) {
            ScreenPrint(10, y, "█"); ScreenPrint(69, y, "█");
        }

        // 트로피 모양
        SetConsoleTextAttribute(hConsole[screenIndex], 14);
        ScreenPrint(54, 11, "  ▄███▄  ");
        ScreenPrint(54, 12, " ▀█████▀ ");
        ScreenPrint(54, 13, "  ▐███▌  ");
        ScreenPrint(54, 14, "   ███   ");
        ScreenPrint(54, 15, "  ▄███▄  ");

        if (animFrame % 20 < 10) SetConsoleTextAttribute(hConsole[screenIndex], 11);
        else SetConsoleTextAttribute(hConsole[screenIndex], 15);
        ScreenPrint(25, 5, "████▄   ▄████▄  ██▄  █  █ ▄█▀");
        ScreenPrint(25, 6, "██▄▄██  ██  ██  ██ █ █  ███▄ ");
        ScreenPrint(25, 7, "██  ▀█▄ ██  ██  ██  ▀█  █  ▀█▄");

        // 난이도명 문자열 포맷팅
        if (selectedDifficulty == 0) strcpy(diffStr, "간술");
        else if (selectedDifficulty == 1) strcpy(diffStr, "회식");
        else if (selectedDifficulty == 2) strcpy(diffStr, "M T");
        else strcpy(diffStr, "만취");

        char modeBuf[100];
        sprintf(modeBuf, "▶ 현재 난이도 랭킹: %s ◀", diffStr);
        SetConsoleTextAttribute(hConsole[screenIndex], 10);
        ScreenPrint(14, 9, modeBuf);

        SetConsoleTextAttribute(hConsole[screenIndex], 8);
        ScreenPrint(14, 10, "------------------------------------------------------");

        if (count == 0) {
            // 저장된 기록이 없을 때 예외 처리
            SetConsoleTextAttribute(hConsole[screenIndex], 7);
            ScreenPrint(16, 15, "해당 난이도에 저장된 기록이 없습니다.");
        }
        else {
            // 최대 5등까지 정렬된 점수 리스트 출력
            for (i = 0; i < 5 && i < count; i++) {
                if (i == 0) SetConsoleTextAttribute(hConsole[screenIndex], 14);      // 1등: 노란색
                else if (i == 1) SetConsoleTextAttribute(hConsole[screenIndex], 15); // 2등: 흰색
                else if (i == 2) SetConsoleTextAttribute(hConsole[screenIndex], 6);  // 3등: 어두운 노랑(동색)
                else SetConsoleTextAttribute(hConsole[screenIndex], 7);              // 나머지: 회색

                char rankSign = '1' + i; // '1', '2', '3'... 문자 동적 할당
                sprintf(buf, "  [%c위]  %5d PTS", rankSign, scores[i]);
                ScreenPrint(16, 13 + (i * 2), buf);

                if (i == 0) ScreenPrint(36, 13, "◀ TOP RECORD"); // 1등 강조
            }
        }

        if (animFrame % 20 < 10) {
            SetConsoleTextAttribute(hConsole[screenIndex], 10);
            ScreenPrint(20, 24, "▶ Press ANY KEY to Return to Menu ◀");
        }

        ScreenFlipping();
        animFrame++;

        if (_kbhit()) {
            _getch();
            break; // 아무 키 입력 시 즉각 루프 탈출
        }
        Sleep(30);
    }
}

// 게임오버 화면
int ShowGameOverScreen(int finalScore) {
    int key;
    int overFrame = 0;
    char scoreStr[100];

    while (true) {
        ScreenClear();
        shakeX = 0; shakeY = 0; // 게임오버 시 화면 움직임 멈춤

        // 테두리 점멸 효과 (빨강 <-> 마젠타)
        if (overFrame % 10 < 5) SetConsoleTextAttribute(hConsole[screenIndex], 12);
        else SetConsoleTextAttribute(hConsole[screenIndex], 13);

        for (int x = 15; x < 65; x++) {
            ScreenPrint(x, 4, "▄"); ScreenPrint(x, 24, "▀");
        }
        for (int y = 5; y < 24; y++) {
            ScreenPrint(15, y, "█"); ScreenPrint(64, y, "█");
        }

        // 해골 모양
        SetConsoleTextAttribute(hConsole[screenIndex], 15);
        ScreenPrint(35, 6, "  ▄████▄  ");
        ScreenPrint(35, 7, " █▀ ▀▀ ▀█ ");
        ScreenPrint(35, 8, " █ ▄  ▄ █ ");
        ScreenPrint(35, 9, " ▀█▄▄▄▄█▀ ");
        ScreenPrint(35, 10, "   █ █    ");

        SetConsoleTextAttribute(hConsole[screenIndex], 12);
        ScreenPrint(20, 12, " ▄████▄   ▄████▄   ▄█    █▄  ████████▄ ");
        ScreenPrint(20, 13, "██       ██    ██  ████████  ██      ██");
        ScreenPrint(20, 14, "██   ▄▄▄ ████████  ██ ▀▀ ██  ██▀▀▀▀▀▀▀ ");
        ScreenPrint(20, 15, " ▀████▀  ██    ██  ██    ██  ████████▀ ");

        ScreenPrint(20, 17, " ▄████▄  ██      ██  ████████  ██▀██▄  ");
        ScreenPrint(20, 18, "██    ██  ██    ██   ██        ██ ▀██▄ ");
        ScreenPrint(20, 19, "██    ██   ██  ██    ██▀▀▀▀▀▀  █████▀  ");
        ScreenPrint(20, 20, " ▀████▀     ▀██▀     ████████  ██  ▀█▄ ");

        SetConsoleTextAttribute(hConsole[screenIndex], 14);
        sprintf(scoreStr, "★ FINAL SCORE : %d POINTS ★", finalScore);
        ScreenPrint(25, 22, scoreStr);

        // 사용자의 다음 선택 제어
        if (overFrame % 20 < 10) {
            SetConsoleTextAttribute(hConsole[screenIndex], 11);
            ScreenPrint(16, 23, "[ 1 ] 바로 재시작  [ 2 ] 메인 메뉴  [ ESC ] 종료");
        }

        ScreenFlipping();
        overFrame++;

        if (_kbhit()) {
            key = _getch();
            if (key == '1') return 1; // 상태코드 1 반환 (즉시 재시작)
            if (key == '2') return 2; // 상태코드 2 반환 (메뉴로 이탈)
            if (key == 27)  return 0; // 상태코드 0 반환 (종료)
        }
        Sleep(30);
    }
}

// 렌더링

// 취함 게이지에 따라 바닥 높이 및 모양 변경
void DrawFloor(int drunk_gauge) {
    if (drunk_gauge >= 80) {
        ScreenPrint(0, 7, "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT");
    }
    else {
        ScreenPrint(0, 25, "________________________________________________________________________________");
    }
}

// 통합 렌더링 함수
void Render(Player p, int difficulty) {
    int i, drawY, objShake, randomColor, px, py, fillBlocks;
    const char* diffStr;
    char scoreTxt[50], gaugeTxt[100], diffTxt[50];
    char gaugeBar[15] = "[          ]"; // 게이지바

    // 1. 모든 오브젝트 렌더링
    for (i = 0; i < MAX_OBJ; i++) {
        if (objects[i].isActive) {
            drawY = objects[i].y; // 기본 y좌표 설정

            // 게이지 40 이상일 때 장애물들이 위아래로 요동치게 만듦
            if ((objects[i].type == TYPE_EXTINGUISHER ||
                objects[i].type == TYPE_TRASHBAG ||
                objects[i].type == TYPE_BOOK) && p.drunk_gauge > 40) {
                objShake = (rand() % 3) - 1; // -1 ~ 1 픽셀 흔들림
                drawY += objShake;
            }

            // 종류별 색상 및 픽셀아트 렌더링
            switch (objects[i].type) {
            case TYPE_EXTINGUISHER:
                SetConsoleTextAttribute(hConsole[screenIndex], 12); // 빨강
                ScreenPrint(objects[i].x, drawY - 2, " ▄■▄ ");
                ScreenPrint(objects[i].x, drawY - 1, " ███ ");
                ScreenPrint(objects[i].x, drawY, " ▀▀▀ ");
                break;
            case TYPE_TRASHBAG:
                SetConsoleTextAttribute(hConsole[screenIndex], 8);  // 회색
                ScreenPrint(objects[i].x, drawY - 2, "  ■  ");
                ScreenPrint(objects[i].x, drawY - 1, " ▄█▄ ");
                ScreenPrint(objects[i].x, drawY, " ▀██▀");
                break;
            case TYPE_BOOK:
                SetConsoleTextAttribute(hConsole[screenIndex], 10); // 초록
                ScreenPrint(objects[i].x, drawY - 2, " ▄▄▄ ");
                ScreenPrint(objects[i].x, drawY - 1, " ███ ");
                ScreenPrint(objects[i].x, drawY, " ███ ");
                break;
            case TYPE_WATER:
                SetConsoleTextAttribute(hConsole[screenIndex], 9);  // 파랑
                ScreenPrint(objects[i].x, drawY - 2, " ▄█▄ ");
                ScreenPrint(objects[i].x, drawY - 1, " ███ ");
                ScreenPrint(objects[i].x, drawY, " ███ ");
                break;
            case TYPE_SOUP:
                SetConsoleTextAttribute(hConsole[screenIndex], 14); // 노랑
                ScreenPrint(objects[i].x, drawY - 2, "  ♨  ");
                ScreenPrint(objects[i].x, drawY - 1, " ▄▄▄ ");
                ScreenPrint(objects[i].x, drawY, " ▀██▀");
                break;
            }
        }
    }

    // 2. 플레이어 렌더링
    // 플레이어 색상 (취함 게이지 50 넘어가면 랜덤 색상으로 깜빡임)
    if (p.drunk_gauge > 50) {
        randomColor = (rand() % 6) + 9;
        SetConsoleTextAttribute(hConsole[screenIndex], randomColor);
    }
    else {
        SetConsoleTextAttribute(hConsole[screenIndex], 15); // 기본 흰색
    }

    px = p.x;
    py = (int)p.y;

    // 만취 바닥 패턴일 때와 아닐 때의 플레이어 모양
    if (p.drunk_gauge >= 80) {
        if (p.isJumping) {
            // 점프 시
            ScreenPrint(px, py - 2, "  ▄▄  ");
            ScreenPrint(px, py - 1, " ▄██▄ ");
            ScreenPrint(px, py, " ▀██▀ ");
        }
        else {
            // 달리기 애니메이션 (점수/프레임 변수를 통해 변화)
            if (p.score % 6 < 3) {
                ScreenPrint(px, py - 2, " ▀▄ ▄▀");
                ScreenPrint(px, py - 1, "  ██  ");
                ScreenPrint(px, py, " ▀██▀ ");
            }
            else {
                ScreenPrint(px, py - 2, "  ▀▀  ");
                ScreenPrint(px, py - 1, "  ██  ");
                ScreenPrint(px, py, " ▀██▀ ");
            }
        }
    }
    else {
        if (p.isJumping) {
            ScreenPrint(px, py - 2, " ▄██▄ ");
            ScreenPrint(px, py - 1, " ▀██▀ ");
            ScreenPrint(px, py, "  ▀▀  ");
        }
        else {
            if (p.score % 6 < 3) {
                ScreenPrint(px, py - 2, " ▄██▄ ");
                ScreenPrint(px, py - 1, "  ██  ");
                ScreenPrint(px, py, " ▄▀ ▀▄");
            }
            else {
                ScreenPrint(px, py - 2, " ▄██▄ ");
                ScreenPrint(px, py - 1, "  ██  ");
                ScreenPrint(px, py, "  ██  ");
            }
        }
    }

    // 3. 게임 정보 (점수, 취함 게이지, 난이도)
    SetConsoleTextAttribute(hConsole[screenIndex], 7);
    sprintf(scoreTxt, "Score: %d", p.score); // 점수 포맷팅

    // 게이지 채우기 (10 단위마다 # 블록 하나씩 추가)
    fillBlocks = p.drunk_gauge / 10;
    if (fillBlocks > 10) fillBlocks = 10;

    for (i = 0; i < fillBlocks; i++) {
        gaugeBar[i + 1] = '#';
    }
    sprintf(gaugeTxt, "Drunk: %s %d / 100", gaugeBar, p.drunk_gauge);

    // 난이도 이름 할당
    if (difficulty == 0) diffStr = "Level: 간술";
    else if (difficulty == 1) diffStr = "Level: 회식";
    else if (difficulty == 2) diffStr = "Level: MT";
    else diffStr = "Level: !!! 만 취 !!!";

    sprintf(diffTxt, "%s", diffStr);

    // 텍스트 화면 좌측 상단에 고정 출력
    ScreenPrint(2, 2, scoreTxt);
    ScreenPrint(2, 3, gaugeTxt);
    ScreenPrint(2, 4, diffTxt);
}


// 시스템 엔진 및 최적화 함수


// 게임 콘솔 창 초기 세팅
void Init() {
    // 커서 숨기기
    CONSOLE_CURSOR_INFO cursorInfo = { 0, };
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = FALSE;

    // 더블 버퍼링을 위한 두 개의 스크린 버퍼 메모리 할당
    hConsole[0] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    hConsole[1] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    SetConsoleCursorInfo(hConsole[0], &cursorInfo);
    SetConsoleCursorInfo(hConsole[1], &cursorInfo);

    // 창 크기를 먼저 정의하고 버퍼 크기를 일치시킴
    SMALL_RECT wSize = { 0, 0, 84, 29 }; // 보이는 창 범위 지정
    SetConsoleWindowInfo(hConsole[0], TRUE, &wSize);
    SetConsoleWindowInfo(hConsole[1], TRUE, &wSize);

    COORD bSize = { 85, 30 }; // 물리적인 메모리상 콘솔 버퍼 범위 강제 고정
    SetConsoleScreenBufferSize(hConsole[0], bSize);
    SetConsoleScreenBufferSize(hConsole[1], bSize);

    // QuickEdit 모드(마우스로 콘솔 드래그 시 스레드가 정지되는 버그) 강제 해제
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prev_mode;
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);

    // 난수 생성기 시드를 현재 시스템 시간으로 초기화
    srand((unsigned int)time(NULL));
}

// 더블 버퍼링 - 유저가 보는 화면과 컴퓨터가 그리는 화면의 포인터 교체
void ScreenFlipping() {
    SetConsoleActiveScreenBuffer(hConsole[screenIndex]);
    screenIndex = !screenIndex; // 0은 1로, 1은 0으로
}

// 화면 백버퍼 지우기
void ScreenClear() {
    COORD co;
    DWORD dw;
    // 0번 행부터 29번 행까지 순회하며 빈 공간(' ')으로 덮어씌움
    for (int y = 0; y < 30; y++) {
        co.X = 0;
        co.Y = y;
        FillConsoleOutputCharacter(hConsole[screenIndex], ' ', 85, co, &dw); // 커널 I/O 덮어쓰기
    }
}

// 원하는 X, Y 좌표에 지정한 문자열을 그리는 함수
void ScreenPrint(int x, int y, const char* string) {
    DWORD dw;
    // 화면 진동을 모든 출력물의 좌표에 일괄 반영
    COORD cursorPosition = { x + shakeX, y + shakeY };

    // 화면 좌표계를 벗어난 마이너스 픽셀 오류 방지 예외 처리
    if (cursorPosition.X < 0) cursorPosition.X = 0;
    if (cursorPosition.Y < 0) cursorPosition.Y = 0;

    // 해당 좌표로 커서를 이동시킨 뒤, C 표준 printf()보다 훨씬 빠른 커널 WriteFile()로 즉시 메모리 출력
    SetConsoleCursorPosition(hConsole[screenIndex], cursorPosition);
    WriteFile(hConsole[screenIndex], string, (DWORD)strlen(string), &dw, NULL);
}