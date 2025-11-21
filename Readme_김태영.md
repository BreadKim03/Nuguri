## 너구리 코드 분석_김태영
- 소스 코드에 주석 형태로 분석하여 서술하였습니다.
- 함수의 로직 및 동작 방식을 중점으로 분석하였으며, 피드백 및 수정 사항은 코멘트를 달아 주세요.

### 주요 개발 사항
- 현재 소스 코드는 목숨(시도 가능 횟수)가 없습니다.조건에는 플레이어의 목숨을 부여하라고 기재되어 있었으므로 전역변수란에 int life = 3;을 선언하여 플레이어의 목숨을 제어해야 합니다.
- 플레이어 낙사 시, 적과 충돌 시 목숨이 차감되도록 로직을 수정해야 합니다.
- 소스 코드는 리눅스를 기준으로 쓰여졌으며, OS 분리 및 코드 수정을 통한 이식성 향상이 필요합니다.
- 게임 타이틀 화면, 엔딩 크레딧 등의 연출 구현이 필요합니다.
- move_player() 함수에서 if (!is_jumping && (floor_tile == '#' || on_ladder)) 구문을 보면, 플레이어가 사다리에 올라타 있더라도(on_ladder = 1) 점프가 가능한 상태가 되는 오류가 존재합니다. 예외 처리 방식을 바꿔야 합니다.


## 이하 코드 전문 및 분석
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
#define MAP_HEIGHT 20 //맵 최대 높이
#define MAX_STAGES 2 //스테이지 개수
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의

//적 유닛
typedef struct {
    int x, y; //좌표
    int dir; // 1: right, -1: left
} Enemy;


//코인
typedef struct {
    int x, y; //좌표
    int collected; //코인의 수집 여부
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];
int player_x, player_y;
int stage = 0;
int score = 0;

// 플레이어 상태
//int 사용하여 여부 판단, 1 = True, 0 = False
int is_jumping = 0; //점프 여부
int velocity_y = 0; //상하 이동 방향(점프 중/하강 중)
int on_ladder = 0; //사다리 사용 여부

// 게임 객체
//선언한 구조체를 토대로 객체 생성
Enemy enemies[MAX_ENEMIES]; //최대 적 개수 = 15
int enemy_count = 0;
Coin coins[MAX_COINS]; //최대 코인 수 = 30
int coin_count = 0;

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

//게임 실행
int main() {
    srand(time(NULL));
    enable_raw_mode();
    load_maps();
    init_stage();

    char c = '\0';
    int game_over = 0;

    //메인 게임 진행 루프
    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) {
            //입력을 통한 제어
            c = getchar();
            if (c == 'q') { //q 입력 시 게임 종료(game over 변수를 이용함)
                game_over = 1;
                continue;
            }
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

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            } else {
                game_over = 1;
                printf("\x1b[2J\x1b[H");
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);
            }
        }
    }

    disable_raw_mode();
    return 0;
}


// 터미널 Raw 모드 활성화/비활성화
//터미널 Raw 모드를 변경하여 키 입력을 실시간으로 받을 지 선택할 수 있음
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 맵 파일 로드
//맵 파일은 map.txt 파일 안에 각 스테이지가 그려져 있음. 스테이지들은 줄바꿈으로 구분되어 있으므로 s값(y값)을 통해 스테이지를 불러옴
void load_maps() {
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }  
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {

        //빈 줄(스테이지 끝) 감지 시 다음 스테이지로 이동
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }

        //각 스테이지의 최대 높이(스테이지 크기) 만큼만 읽음
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

    //적, 코인, 상태 변수 전체 초기화
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;

    //맵 크기만큼 반복문 실행
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') {
                //스테이지 시작 시점 플레이어 위치
                player_x = x;
                player_y = y;
            } else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){x, y, (rand() % 2) * 2 - 1}; //적 유닛 생성 후 좌우 이동 방향 무작위 지정(난수%2 나머지 연산으로 0~1의 수 생성 후 연산을 통해 1/-1의 두 개의 수를 만들어 방향 결정)
                enemy_count++; //적 유닛 수++
            } else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){x, y, 0}; //코인 생성
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    printf("\x1b[2J\x1b[H"); //화면 지우고 커서 위치 조정
    printf("Stage: %d | Score: %d\n", stage + 1, score); //스테이지, 점수 표기
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n"); //조작키 설명 표시

    //출력할 맵 배열 생성 및 요소 복사
    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for(int y=0; y < MAP_HEIGHT; y++) {
        for(int x=0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            //S, X, C는 공백으로 대체함. 나중에 별도로 데이터를 읽어서 반영하는 방식
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            } else {
                display_map[y][x] = cell;
            }
        }
    }
    
    //출력할 맵 배열에 코인 반영
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) { //수집되지 않은 코인만 출력할 맵 배열에 반영
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    //출력할 맵 배열에 적 유닛 반영
    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    //출력할 맵 배열에 플레이어 반영
    display_map[player_y][player_x] = 'P';

    //맵 배열을 토대로 화면에 출력
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for(int x=0; x< MAP_WIDTH; x++){
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input); //플레이어 이동 처리
    move_enemies(); //적 유닛 이동
    check_collisions(); //충돌 판정
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y; //플레이어의 다음 좌표 초기화
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    //플레이어 좌표(배열의 좌표 == 사다리)가 사다리인 경우 사다리 상태 True
    on_ladder = (current_tile == 'H');

    //입력에 따라 플레이어의 다음 좌표 조정, 상하 좌표의 경우 사다리에 있는지 여부를 먼저 판단
    switch (input) {
        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') next_y++; break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
            }
            break;
    }

    //플레이어의 x좌표가 맵의 경계에 있는지 판별
    if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') player_x = next_x;
    
    //플레이어의 y좌표가 사다리의 경계에 있는지 판별
    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 

    //점프 중인지 판별
    else {
        //점프 중인 경우 velocity y = 1
        if (is_jumping) {
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            velocity_y++;

            //위쪽이 벽인 경우 점프 중단
            if (velocity_y < 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#') {
                velocity_y = 0;
            } else if (next_y < MAP_HEIGHT) {
                player_y = next_y;
            }
            
            //착지
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        } 
        
        //중력 적용
        else {
            if (floor_tile != '#' && floor_tile != 'H') {
                 if (player_y + 1 < MAP_HEIGHT) player_y++;
                 else init_stage(); //낙사(바닥 없음)
            }
        }
    }
    

    //플레이어의 y좌표가 맵을 벗어난 경우 스테이지 재시작(버그 감지 로직인 것으로 추정)
    if (player_y >= MAP_HEIGHT) init_stage();
}


// 적 이동 로직
//기본적으로 좌/우를 반복해서 이동하는 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        //벽이나 낭떠러지를 만나면 방향 전환
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
    //적과 충돌했을 경우 판별
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            score = (score > 50) ? score - 50 : 0; //적과 충돌했을 경우 점수 -50
            init_stage(); //스테이지 재시작
            return;
        }
    }

    //코인과 충돌했을 경우 판별
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1; //코인과 충돌했을 경우 코인의 획득 변수를 조정하여 화면에서 제거
            score += 20; //점수 +20
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
} ```