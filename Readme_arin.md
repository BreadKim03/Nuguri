## Nuguri_arin
```
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

// 맵 및 게임 요소 정의 (수정된 부분)
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20 //맵 높이
#define MAX_STAGES 2 //총 스테이지 개수
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
//적 유닛 구조체
typedef struct {
    int x, y; //적 좌표
    int dir; // 이동 1: right, -1: left
} Enemy;

//코인 구조체
typedef struct {
    int x, y; //코인 좌표
    int collected; //수집 여부 (1: 수집됨/안보임, 0: 수집안됨/보임)
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1]; //맵 데이터 저장 배열
int player_x, player_y; //플레이어 현재 좌표
int stage = 0; //현재 진행중인 스테이지
int score = 0; //현재 점수

// 플레이어 상태(0:true, 1:false)
int is_jumping = 0; //현재 점프 중인지 여부
int velocity_y = 0; //위아래 이동(음수: 위로 상승, 양수: 아래로 하강)
int on_ladder = 0; //사다리에 매달려 있는지 여부

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0; //현재 스테이지의 적 개수
Coin coins[MAX_COINS];
int coin_count = 0; //현재 스테이지의 코인 개수

// 터미널 설정
struct termios orig_termios;

// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input);
void move_player(char input);
void move_enemies();
void check_collisions();
int kbhit();

//메인 함수
int main() {
    srand(time(NULL)); //난수 생성 초기화 (적 랜덤 생성을 위해)
    enable_raw_mode(); //터미널을 raw 모드로 변경 (엔터 없이 즉시 입력 받음)
    load_maps(); //외부 텍스트 파일에서 맵 로딩
    init_stage(); // 첫 스테이지 객체 배치

    char c = '\0'; //입력받은 키 값 저장
    int game_over = 0; //게임 종료(1:종료, 0:진행중)

    //게임 루프
    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) { //키보드 입력 확인
            c = getchar(); //입력된 키 읽기
            if (c == 'q') {
                game_over = 1; //q 누르면 게임 종료
                continue;
            }
            //방향키 처리
            if (c == '\x1b') {
                getchar(); // '['
                switch (getchar()) {
                    case 'A': c = 'w'; break; // Up
                    case 'B': c = 's'; break; // Down
                    case 'C': c = 'd'; break; // Right
                    case 'D': c = 'a'; break; // Left
                }
            }
        } else {
            c = '\0';
        }

        update_game(c);
        draw_game();
        usleep(90000);

        //스테이지 클리어 체크
        if (map[stage][player_y][player_x] == 'E') {
            stage++; //다음 스테이지
            score += 100; 
            if (stage < MAX_STAGES) {
                init_stage(); //스테이지 초기화
            } else {
                game_over = 1; //모든 스테이지 클리어
                printf("\x1b[2J\x1b[H"); // 화면 지우기
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);
            }
        }
    }

    disable_raw_mode();
    return 0;
}


// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 맵 파일 로드
void load_maps() {
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0; //s:스테이지, r:높이
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
        //빈 줄 발견 시 다음 스테이지로 넘어감
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0; 
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

    //맵을 순회하며 플레이어(S), 적(X), 코인(C) 위치 파악
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') { //시작 위치
                player_x = x;
                player_y = y;
            } else if (cell == 'X' && enemy_count < MAX_ENEMIES) { //적 배치
                enemies[enemy_count] = (Enemy){x, y, (rand() % 2) * 2 - 1};
                enemy_count++;
            } else if (cell == 'C' && coin_count < MAX_COINS) {//코인 배치
                coins[coin_count++] = (Coin){x, y, 0};
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    printf("\x1b[2J\x1b[H"); //화면 전체 지우고 커서 이동
    printf("Stage: %d | Score: %d\n", stage + 1, score);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    //출력할 임시 맵 생성
    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for(int y=0; y < MAP_HEIGHT; y++) {
        for(int x=0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' '; //S,X,C는 공백 처리
            } else {
                display_map[y][x] = cell; //벽이나 사다리는 그대로 복사
            }
        }
    }
    
    //코인(수집 되지 않은 것만)
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) { 
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    //적
    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    //플레이어
    display_map[player_y][player_x] = 'P';

    //최종 맵 출력
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for(int x=0; x< MAP_WIDTH; x++){
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input); //플레이어 이동
    move_enemies(); //적 이동
    check_collisions(); //충돌 체크(적, 코인)
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    //바로 아래칸 바닥인지 확인
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    //현재 사다리에 있는지 여부
    on_ladder = (current_tile == 'H');

    switch (input) { //키 입력 처리
        case 'a': next_x--; break; //좌
        case 'd': next_x++; break; //우
        case 'w': if (on_ladder) next_y--; break; //사다리 위일 때만 상승가능
        //사다리 위이고 바닥이 아닐 때 하강 가능
        case 's': if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') next_y++; break;
        case ' ': //점프중이 아니고, 바닥에 있거나 사다리에 있을 때만 점프 가능
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
            }
            break;
    }

    //좌우 벽 충돌 체크(벽이 아닐때만 x좌표 갱신)
    if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') player_x = next_x;
    
    //사다리 이동 처리
    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 
    else {
        if (is_jumping) { //점프 중인지 확인
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            velocity_y++;

            //천장 충돌 체크
            if (velocity_y < 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#') {
                velocity_y = 0;
            } else if (next_y < MAP_HEIGHT) {
                player_y = next_y;
            }
            
            //바닥 착지 체크
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') {
                is_jumping = 0; //점프 종료
                velocity_y = 0;
            }
        } else { //바닥이 없으면 추락
            if (floor_tile != '#' && floor_tile != 'H') {
                 if (player_y + 1 < MAP_HEIGHT) player_y++;
                 else init_stage(); //맵 밖으로 떨어지면 죽음
            }
        }
    }
    //맵 아래로 벗어나면 재시작
    if (player_y >= MAP_HEIGHT) init_stage();
}


// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        //맵 끝, 벽, 낭떠러지 만나면 방향 전환
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1; //방향 전환
        } else {
            enemies[i].x = next_x; 
        }
    }
}

// 충돌 감지 로직
void check_collisions() {
    //적과 플레이어가 만났을 때
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            score = (score > 50) ? score - 50 : 0; //점수 차감
            init_stage(); //스테이지 재시작
            return;
        }
    }
    //플레이어와 코인이 만났을 때
    for (int i = 0; i < coin_count; i++) {
        //먹지 않은 코인과 좌표가 같으면 획득
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1; //수집됨 상태로 변경
            score += 20; //점수 증가
        }
    }
}

// 비동기 키보드 입력 확인
int kbhit() {
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
```
