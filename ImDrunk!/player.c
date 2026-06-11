#include "player.h"


// 1. 물리 엔진 정의

// 화면 좌측 상단이 (0,0)이고, 아래로 내려갈수록 Y값이 증가하는 좌표 사용
#define GROUND_Y 24.0f   // 평상시 바닥 높이 (가장 아래쪽)
#define FLOAT_Y 10.0f    // MT/만취 모드에서 취했을 때 떠오르는 천장 높이
#define GRAVITY 0.8f     // 정상 중력 (매 프레임마다 Y값 증가)
#define JUMP_POWER -3.0f // 정상 점프력 (순간적으로 음수 값을 주어 위로 솟구치게 함)


// 2. 플레이어 초기화 함수

// 게임이 시작될 때 플레이어의 초기 상태 세팅
void Player_Init(Player* p) {
    p->x = 5;               // 화면 왼쪽에서 약간 떨어진 곳에 고정
    p->y = GROUND_Y;        // 처음에는 바닥에 위치
    p->velocity_y = 0.0f;   // 초기 수직 방향 속도는 0 (정지 상태)
    p->isJumping = false;   // 현재 점프 중인지 확인하는 상태 플래그 (더블 점프 방지용)
    p->score = 0;           // 점수 초기화
    p->drunk_gauge = 0;     // 취함 게이지 초기화
}


// 3. 점프 이벤트 함수

// 스페이스바 입력이 들어왔을 때 단 한 번만 호출되어 초기 속도를 부여함
void Player_Jump(Player* p) {
    // isJumping이 false일 때만 점프 허용(더블 점프 방지)
    if (!p->isJumping) {
        p->isJumping = true; // 상태를 점프 중으로 변경

        // [게이지 80 이상]
        // 천장에 매달려 있는 상태이므로, 스페이스바를 누르면 오히려 아래로 점프
        if (p->drunk_gauge >= 80) {
            p->velocity_y = 3.0f; // 양수 방향으로 초기 속도를 주어 아래로 꽂히게 만듦
        }
        // [정상 상태]
        else {
            p->velocity_y = JUMP_POWER; // 음수 방향으로 초기 속도를 주어 위로 솟구치게 만듦
        }
    }
}


// 4. 업데이트 함수 (프레임 단위로 계속 호출)

// 매 프레임마다 위치 = 위치 + 속도, 속도 = 속도 + 가속도를 계산함
void Player_Update(Player* p) {

    // [A. 반중력 (MT/만취) 상태 로직] - 취함 게이지 80 이상
    if (p->drunk_gauge >= 80) {

        // 1. 공중 부양 로직
        // 점프 중이 아닌데 바닥 쪽에 있다면, 목표 높이(FLOAT_Y)까지 천천히 위로 끌어올림
        if (!p->isJumping && p->y > FLOAT_Y) {
            p->y -= 1.5f; // 위로 이동 (Y값 감소)

            // 목표 높이를 지나치면 튕겨나가는 걸 방지하기 위해 값을 FLOAT_Y로 묶어둠
            if (p->y < FLOAT_Y) p->y = FLOAT_Y;
        }

        // 2. 역점프 물리 계산
        if (p->isJumping) {
            p->y += p->velocity_y;  // 현재 속도만큼 위치를 이동
            p->velocity_y -= 0.4f;  // 아래로 꽂히던 속도를 서서히 위로(음수 방향) 끌어당김

            // 다이빙 하다가 밑바닥을 뚫고 나가지 않도록 바닥 높이에서 차단
            if (p->y >= GROUND_Y) p->y = GROUND_Y;

            // 천장(FLOAT_Y)으로 다시 돌아왔고, 속도가 위로 향하고(음수) 있다면 역점프 종료 처리
            if (p->y <= FLOAT_Y && p->velocity_y < 0) {
                p->y = FLOAT_Y;        // 위치를 천장에 고정
                p->isJumping = false;  // 점프 상태 해제 (다시 스페이스바 입력 대기)
                p->velocity_y = 0.0f;  // 속도 0으로 초기화
            }
        }
    }

    // [B. 정상 상태 로직] - 취함 게이지 80 미만

    else {

        // 1. 추락 로직 (술이 깨서 다시 바닥으로 내려옴)
        // 점프 중이 아닌데 공중에 떠 있다면, 원래 바닥(GROUND_Y)으로 천천히 끌어내림
        if (!p->isJumping && p->y < GROUND_Y) {
            p->y += 2.0f; // 아래로 이동 (Y값 증가)

            // 바닥을 지나치면 값을 GROUND_Y로 묶어둠
            if (p->y > GROUND_Y) p->y = GROUND_Y;
        }

        // 2. 일반 점프 물리 계산
        if (p->isJumping) {
            p->y += p->velocity_y;    // 현재 속도만큼 위치를 위(음수 방향)로 이동
            p->velocity_y += GRAVITY; // 정상 중력 작용 => 위로 솟구치던 속도를 서서히 아래로(양수 방향) 당김

            // 점프 후 바닥(GROUND_Y)에 닿거나 뚫고 내려가려 하면 착지 처리
            if (p->y >= GROUND_Y) {
                p->y = GROUND_Y;       // 위치를 바닥에 고정
                p->isJumping = false;  // 점프 상태 해제
                p->velocity_y = 0.0f;  // 속도 0으로 초기화
            }
        }
    }
}