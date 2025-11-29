#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <time.h>
#include <conio.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#endif

// 맵 및 게임 요소 정의 (수정된 부분)
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20
#define MAX_STAGES 2
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

//게임 결과
static struct result
{
    char name[30];
    long point;
    int year;
    int month;
    int day;
    int hour;
    int min;
    int rank;
}temp_result;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];
int player_x, player_y;
int stage = 0;
int score = 0;
int lives = 3; //플레이어 목숨 변수 추가

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

#ifdef _WIN32
DWORD oldt;
#else
// 터미널 설정
struct termios orig_termios;
#endif

// 함수 선언
void msleep(int t);
void clrscr();
void disable_raw_mode();
void enable_raw_mode();
int mkbhit();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input);
void move_player(char input);
void move_enemies();
void check_collisions();
void title();
void victory();
void credit();
void printstage(int stage);
void Savedata(char* name);
void credit();

//메인 메뉴
void title()
{
    printf("\n\n\n\n\n\n\n                              T");
    msleep(500);
    printf("    H");
    msleep(500);
    printf("    E");
    msleep(500);
    printf("\n\n            P");
    msleep(500);
    printf("    O");
    msleep(500);
    printf("    M");
    msleep(500);
    printf("    P");
    msleep(500);
    printf("    O");
    msleep(500);
    printf("    M");
    msleep(500);
    printf("    P");
    msleep(500);
    printf("    O");
    msleep(500);
    printf("    K");
    msleep(500);
    printf("    O");
    msleep(5000);
    return;
}

void printstage(int stage)
{
    clrscr();
    printf("\n\n\n\n\n\n\n                              Stage %d", stage + 1);
    msleep(5000);
}

void credit()
{
    FILE* fp;
    struct result* records;
    long num_records = 0;
    struct result current_record;
    char name[20];
    char date[11];
    char time[6];
    long score;
    int year, month, day, hour, min;

    clrscr();
    fp = fopen("PlayerRecords.txt", "r");

    if (fp == NULL)
    {
        printf("기록이 없거나 불러오는 데 실패했습니다.");
        msleep(4000);
        return;
    }

    printf("\n===========전체 기록===========\n");
    while (fscanf(fp, "%s %ld %d-%d-%d %d시 %d분", name, &score, &year, &month, &day, &hour, &min) == 7)
    {
        printf("\n%s 점수: %ld, 날짜: %d-%d-%d %d시 %d분\n", name, score, year, month, day, hour, min);
    }

    fclose(fp);

}

void Savedata(char* name)
{
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    FILE* fp;

    struct result tmp;
    strcpy(tmp.name, name);
    tmp.point = score;
    tmp.year = tm->tm_year + 1900;
    tmp.month = tm->tm_mon + 1;
    tmp.day = tm->tm_mday;
    tmp.hour = tm->tm_hour;
    tmp.min = tm->tm_min;

    fp = fopen("PlayerRecords.txt", "a");
    if (fp == NULL)
    {
        printf("기록 실패");
        return;
    }

    fprintf(fp, "%s %ld %d-%d-%d %d시 %d분\n", tmp.name, tmp.point, tmp.year, tmp.month, tmp.day, tmp.hour, tmp.min);
    printf("\n\n\n					      당신의 기록이 저장되었습니다!");
    fclose(fp);
    Sleep(1000);
}

//게임 승리
void victory()
{
    clrscr();
    printf("\n\n\n\n\n\n\n\n\n\n                    Congratulations! You are Win!");
    msleep(3000);
}

int main() {
    srand(time(NULL));
    enable_raw_mode();
    title();
    load_maps();
    init_stage();
    printstage(stage);

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
#ifdef _WIN32
        if (mkbhit()) {
            int k = _getch();
            if (k != 0 && k != 0xE0)
            {
                c = (char)k;

                if (c == 'q')
                {
                    game_over = 1;
                    continue;
                }
            }
            else
            {
                k = _getch();
                switch (k) {
                case 72: c = 'w'; break; // Up
                case 80: c = 's'; break; // Down
                case 77: c = 'd'; break; // Right
                case 75: c = 'a'; break; // Left
                }
            }
        }
        else {
            c = '\0';
        }
#else
        if (mkbhit()) {
            c = getchar();
            if (c == 'q') {
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
        }
        else {
            c = '\0';
        }
#endif
        update_game(c);
        draw_game();
        msleep(90);

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            }
            else {
                game_over = 1;
                victory();
                clrscr();
                printf("게임을 클리어하는 데 %d번의 목숨을 사용하였습니다.\n", 3 - lives);
                printf("최종 점수: %d\n", score);
                printf("플레이어 이름을 입력해주세요 : ");
                char name[10];
                scanf("%s", &name);
                Savedata(name);
                credit();
            }
        }
        if (lives <= 0) {
            game_over = 1;
            credit();
        }
    }

    disable_raw_mode();
    return 0;
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
            if (cell == 'S') {
                player_x = x;
                player_y = y;
            }
            else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count].x = x;
                enemies[enemy_count].y = y;
                enemies[enemy_count].dir = (rand() % 2) * 2 - 1;
                enemy_count++;
            }
            else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count].x = x;
                coins[coin_count].y = y;
                coins[coin_count].collected = 0;
                coin_count++;
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    clrscr();
    printf("Stage: %d | Score: %d\n | Lives: %d\n", stage + 1, score, lives);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            }
            else {
                display_map[y][x] = cell;
            }
        }
    }

    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input);
    move_enemies();
    check_collisions();
}

// 플레이어 이동 로직
void move_player(char input) { // 키 입력(input)을 받아 플레이어 위치 바꾸는 함수
    int next_x = player_x; // 이동 후 x좌표
    int next_y = player_y; // 이동 후 y좌표

    // 현재 타일 기준 상태
    char current_tile = map[stage][player_y][player_x]; // 플레이어가 지금 서 있는 칸의 문자
    on_ladder = (current_tile == 'H'); // 현재 칸이 사다리면 1, 아니면 0

    // 이동 기능 입력
    switch (input) {
    case 'a': // 왼쪽
        next_x--;
        break;
    case 'd': // 오른쪽
        next_x++;
        break;
    case 'w': // 사다리에서 위로
        if (on_ladder) next_y--;
        break;
    case 's': // 사다리에서 아래로
        if (on_ladder && // 사다리에 있고
            player_y + 1 < MAP_HEIGHT && // 아래로 한 칸 내려가도 맵 범위를 넘지 않게
            map[stage][player_y + 1][player_x] != '#') { // 아래 칸이 벽이 아니면
            next_y++; // 이동
        }
        break;
    case ' ':
        // 바닥이나 사다리 위에 있을 때만 점프
        char below;

        if (player_y + 1 < MAP_HEIGHT) {
            below = map[stage][player_y + 1][player_x];
        }
        else {
            below = '#';   // 맵 아래는 바닥 취급
        }

        // 점프 중은 아니고 바닥 또는 사다리 위일 때 점프 가능
        if (!is_jumping && (below == '#' || on_ladder)) {
            is_jumping = 1;
            velocity_y = -2;
        }
        break;
    default:
        break;
    }

    // 좌우로 이동할 때 맵 안에서만 이동 가능
    if (next_x != player_x) {
        if (next_x >= 0 && next_x < MAP_WIDTH && // x좌표 맵 범위 안에 있고
            map[stage][player_y][next_x] != '#') { // 그 칸이 벽이 아니라면
            player_x = next_x; // 좌우 이동
        }
    }

    // player가 있는 타일 타입
    current_tile = map[stage][player_y][player_x]; // 좌우 이동 후, 현재 플레이어가 있는 칸 읽기
    on_ladder = (current_tile == 'H'); // 이동 후에 사다리 위인지

    char floor_tile;
    if (player_y + 1 < MAP_HEIGHT) {
        // 발 아래 칸이 맵 범위 안이면 실제 타일을 읽는다
        floor_tile = map[stage][player_y + 1][player_x];
    }
    else {
        // 맵 아래로 벗어나면 바닥으로 취급
        floor_tile = '#';
    }

    // 사다리에서 위아래로 이동
    if (on_ladder && (input == 'w' || input == 's')) {
        // 현재 사다리 위에 있으면서 w 또는 s 입력이 들어온 경우에만 사다리에서만 위아래 이동 허용
        if (next_y >= 0 && next_y < MAP_HEIGHT && // y좌표 후보가 맵 범위 안이고
            map[stage][next_y][player_x] != '#') { // 이동하려는 칸이 벽이 아니면
            player_y = next_y; // 이동
            is_jumping = 0; // 점프 상태 해제
            velocity_y = 0; // 점프 속도 0으로 초기화
        }
    }
    else {
        // 점프 중이면
        if (is_jumping) {
            next_y = player_y + velocity_y; // 현재 속도만큼 y좌표 후보 계산
            // 화면 위로 나가는 것 방지
            if (next_y < 0) {
                next_y = 0;
            }

            if (velocity_y < 0) { // 위로 올라가는 중
                // 머리 위가 막혀 있으면 더 이상 못 감
                if (next_y >= 0 && next_y < MAP_HEIGHT &&
                    map[stage][next_y][player_x] != '#') { // 위쪽 칸이 벽이 아니면
                    player_y = next_y; // 이동
                }
                else {
                    // 천장에 부딪히면 위로 이동 중단
                    velocity_y = 0;
                }
                velocity_y++;
            }
            else {
                // 아래로 떨어질 때 한 칸씩만 떨어지면서 바닥 체크
                floor_tile =
                    (player_y + 1 < MAP_HEIGHT)
                    ? map[stage][player_y + 1][player_x]
                    : '#';

                    if (floor_tile != '#') { // 발 아래가 바닥이 아니면
                        if (player_y + 1 < MAP_HEIGHT) {
                            player_y++; // 한 칸 아래로 떨어짐
                        }
                        else {
                            lives--;
                            if (lives > 0) {
                                init_stage(); // 맵 아래로 완전히 떨어지면 스테이지 리셋
                            }
                            return;
                        }
                    }
                    else {
                        // 발 아래가 바로 바닥이면 착지
                        is_jumping = 0;
                        velocity_y = 0;
                    }
                    velocity_y++; // 중력이 커지면서 낙하 속도 증가
            }
        }
        // 점프 상태가 아닌 기본 중력 상태
        else {
            if (floor_tile != '#' && floor_tile != 'H') {
                if (player_y + 1 < MAP_HEIGHT) {
                    player_y++; // 한 칸씩 아래로 떨어짐
                }
                else {
                    lives--;
                    if (lives > 0) {
                        init_stage(); // 맵 아래로 떨어지면 스테이지 리셋
                    }
                    return;
                }
            }
        }
    }

    // 만약 맵 아래로 완전히 나가게 되면 스테이지 리셋
    if (player_y >= MAP_HEIGHT) {
        lives--;
        if (lives > 0) {
            init_stage();
        }
    }
}

// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        }
        else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions() {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            score = (score > 50) ? score - 50 : 0;
            lives--;
            if (lives > 0) {
                init_stage();
            }
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

// 맵 파일 로드
void load_maps() {
    FILE* file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
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

#ifdef _WIN32
void clrscr()
{
    system("cls");
}

void msleep(int t)
{
    Sleep(t);
}

void disable_raw_mode()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hStdin, oldt);
}

void enable_raw_mode()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &oldt);

    DWORD newt = oldt;

    newt &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    newt &= ~(ENABLE_PROCESSED_INPUT);
    SetConsoleMode(hStdin, newt);
    atexit(disable_raw_mode);
}

int mkbhit()
{
    return _kbhit();
}
#else

void msleep(int t)
{
    usleep(t * 1000);
}

// 비동기 키보드 입력 확인
int mkbhit() {
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
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void clrscr()
{
    printf("\x1b[2J\x1b[H");
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
#endif
