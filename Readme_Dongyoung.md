#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
// 크로스 플랫폼(Windows) #ifdef_WIN32 추가

// 맵 및 게임 요소 정의 (수정된 부분)
// 전역 변수, 자료형 아님 -> 매크로 치환 값
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경, 맵 가로 길이
#define MAP_HEIGHT 20 // 맵 세로 길이
#define MAX_STAGES 2 // map.txt보면 스테이지 두 개 있음
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
typedef struct { // 적 구조체
    int x, y; // 적 위치
    int dir; // 방향 이동(1: right, -1: left)
} Enemy;

typedef struct { // 코인 구조체
    int x, y; // 코인 좌표
    int collected; // 코인 먹었는지 확인
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];// 2차원 맵들 저정하기
int player_x, player_y; // 플레이어 위치 
int stage = 0; // 현재 스테이지 번호
int score = 0; // 점수

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES]; // Enemy 구조체 배열
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

// 터미널 설정
struct termios orig_termios;

// 함수 선언
void disable_raw_mode(); // 원래 입력 모드로 되돌리기
void enable_raw_mode(); // 즉시 입력 모드
void load_maps(); // map.txt
void init_stage(); // 스테이지 초기화
void draw_game(); // 게임 화면 출력
void update_game(char input); // 게임 업데이트
void move_player(char input); // 플레이어 이동 함수
void move_enemies(); // 적 이동 함수
void check_collisions(); // 적과 코인 충돌 관련 함수
int kbhit(); // 비동기 키 입력 확인

int main() {
    srand(time(NULL)); // 랜덤 초기화
    enable_raw_mode(); // 즉시 입력 모드 실행
    load_maps(); // 
    init_stage(); // 첫 스테이지 초기화

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) { // 키 입력 있는지 확인하고
            c = getchar();
            if (c == 'q') { // q 누르면 종료
                game_over = 1;
                continue;
            }
            if (c == '\x1b') { // ANSI 코드로 방향키 입력
                getchar(); // '[' 문자 버림
                switch (getchar()) {
                    case 'A': c = 'w'; break; // Up
                    case 'B': c = 's'; break; // Down
                    case 'C': c = 'd'; break; // Right
                    case 'D': c = 'a'; break; // Left
                }
            }
        } else {
            c = '\0'; // 입력 없을 때는 빈 입력
        }

        update_game(c); // 게임 업데이트
        draw_game(); // 화면 그리기
        usleep(90000);// 화면 대기 시간

        if (map[stage][player_y][player_x] == 'E') { // 출구(E)에 도달하면
            stage++; // 스테이지 증가
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            } else {
                game_over = 1;
                printf("\x1b[2J\x1b[H");
                // \x1b = ESC 에스케이프 문자
                // [2J = 화면 전체 지우기
                // [H = 커서 이동
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);
            }
        }
    }

    disable_raw_mode(); // 원래 입력 모드로 되돌리기
    return 0;
}


// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode() { 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); } // 초기 설정 복구
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios); // 원래 설정 저장
    atexit(disable_raw_mode); // 종료 시 복구 등록
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // 입력 버퍼링 + 에코 제거?
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 맵 파일 로드 (map.txt)
void load_maps() {
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) { // 빈 줄 발견 시 다음 스테이지로 전환
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0; // 줄바꿈 제거
            strncpy(map[s][r], line, MAP_WIDTH + 1);
            r++;
        }
    }
    fclose(file);
}


// 현재 스테이지 초기화
void init_stage() {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') { // 플레이어 시작점
                player_x = x;
                player_y = y;
            } else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){x, y, (rand() % 2) * 2 - 1};
                enemy_count++;
            } else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){x, y, 0};
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    printf("\x1b[2J\x1b[H"); // 화면 지우기 + 커서 이동
    printf("Stage: %d | Score: %d\n", stage + 1, score);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for(int y=0; y < MAP_HEIGHT; y++) {
        for(int x=0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            } else {
                display_map[y][x] = cell;
            }
        }
    }
    
    // 코인 다시 표시
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    // 적 표시
    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    // 플레이어 표시
    display_map[player_y][player_x] = 'P';

    // 최종 출력
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for(int x=0; x< MAP_WIDTH; x++){
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input); // 플레이어 이동
    move_enemies(); // 적 이동
    check_collisions(); // 적과 코인 충돌 확인
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H'); // 현재 위치가 사다리?

    switch (input) {
        case 'a': next_x--; break; // 왼쪽 이동
        case 'd': next_x++; break; // 오른쪽 이동
        case 'w': // 위로 이동
         if (on_ladder) next_y--; break; 
        case 's': // 아래로 이동
         if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') next_y++; break;
        case ' ': // 스페이스로 점프
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
            }
            break;
    }

    // 좌우 충돌 체크 후 이동, next_x >= 0 : 왼쪽 벽 넘으면 안됨
    if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') player_x = next_x;
    
    // 사다리 위에서의 상하 이동
    // current_title == 'H'
    // w, s를 눌렀을 때만 실행
    // 사다리 상태에서 중력 무시하기 때문에 점프 상태 초기화
    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 
    else {
        if (is_jumping) {
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            velocity_y++;

            if (velocity_y < 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#') {
                velocity_y = 0;
            } else if (next_y < MAP_HEIGHT) { // 벽이 없으면 y좌표 변경
                player_y = next_y;
            }
            
            // #은 벽. 땅 위에 있으면 점프 상태 끝
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        } else {
            if (floor_tile != '#' && floor_tile != 'H') {
                 if (player_y + 1 < MAP_HEIGHT) player_y++;
                 else init_stage();
            }
        }
    }
    
    if (player_y >= MAP_HEIGHT) init_stage();
}


// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        } else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions() {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            score = (score > 50) ? score - 50 : 0;
            init_stage();
            return;
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
        }
    }
}

// 비동기 키보드 입력 확인
int kbhit() { // Windows용 OS분리
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}