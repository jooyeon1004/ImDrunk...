#include "player.h"

#define GROUND_Y 24.0f  // 바닥의 Y 좌표 (플레이어가 평소 서 있는 위치)
#define GRAVITY 0.8f    // 중력 값
#define JUMP_POWER -4.0f // 점프 시 순간적인 위쪽 방향 속도

void Player_Init(Player* p) {
    p->x = 5; // 왼쪽 아래에 고정 (X축 5)
    p->y = GROUND_Y;
    p->velocity_y = 0.0f;
    p->isJumping = false;
    p->score = 0;
    p->drunk_gauge = 0;
}

void Player_Jump(Player* p) {
    // 공중에 떠 있지 않을 때(바닥에 있을 때)만 점프 가능
    if (!p->isJumping) {
        p->isJumping = true;
        p->velocity_y = JUMP_POWER; // 위로 튀어 오름
    }
}

void Player_Update(Player* p) {
    // 점프 중일 때 중력 물리 로직 적용
    if (p->isJumping) {
        p->y += p->velocity_y;     // 속도만큼 y좌표 이동
        p->velocity_y += GRAVITY;  // 속도에 중력을 더해서 점점 떨어지게 만듦

        // 다시 바닥에 닿았을 때
        if (p->y >= GROUND_Y) {
            p->y = GROUND_Y;
            p->isJumping = false;
            p->velocity_y = 0.0f;
        }
    }
}