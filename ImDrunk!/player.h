#ifndef PLAYER_H
#define PLAYER_H
#include <stdbool.h>

// 플레이어 구조체 정의
typedef struct {
    int x;
    float y;          // 중력을 부드럽게 적용하기 위해 float 사용
    float velocity_y; // Y축 이동 속도
    bool isJumping;   // 점프 중인지 확인하는 상태값
    int score;
    int drunk_gauge;
} Player;

// 플레이어 관련 함수 선언
void Player_Init(Player* p);
void Player_Jump(Player* p);
void Player_Update(Player* p);

#endif#pragma once
