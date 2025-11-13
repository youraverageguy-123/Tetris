#include "raylib.h"
#define FOG (Color){ 100, 100, 100, 100 } // semi-transparent gray fog
#include<math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
static const int CELL = 26; // pixel size for cells
static const int GRID_X = 40; // left margin
static const int GRID_Y = 80; // top margin

#define ROWS 20
#define COLS 15
#define TRUE 1
#define FALSE 0
#define SCORE_FILE "scores.txt"
#define MAX_PLAYERS 200
#define MIN_TIMER_MS 50
#define BASE_TIMER_MS 400
#define PLAYER_FILE "players.txt"
#define MAX_PROFILES 200



/* Forward prototypes (kept mostly same names so logic remains intact) */
void showScores(void);
void clearScores(void);
void saveScore(const char *name, int score_v);
void drawGridAndCurrent(void);
void drawNextPreview(int px, int py);
void WriteToTable(void);
void RotateCurrentIfPossible(void);


/* --- Globals reused from your console version --- */
char Table[ROWS][COLS];
int score = 0;
int GameOn = TRUE;
int timer_ms = BASE_TIMER_MS;
int level = 0;

/* --- SHAPES --- */
static const signed char S0_0[3][3] = {{0,1,1},{1,1,0},{0,0,0}};
static const signed char S1_0[3][3] = {{1,1,0},{0,1,1},{0,0,0}};
static const signed char S2_0[3][3] = {{0,1,0},{1,1,1},{0,0,0}};
static const signed char S3_0[3][3] = {{0,0,1},{1,1,1},{0,0,0}};
static const signed char S4_0[3][3] = {{1,0,0},{1,1,1},{0,0,0}};
static const signed char S5_0[2][2] = {{1,1},{1,1}};
static const signed char S6_0[4][4] = {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}};
const int ShapeWidths[7] = {3,3,3,3,3,2,4};

/* --- STRUCTS --- */
typedef struct {
    char **array;
    int width;
    int row, col;
} Shape;

typedef struct {
    char name[50];
    int score;
} Player;
typedef struct {
    char name[50];
    int bestScore;
} PlayerProfile;


/* --- LINKED LIST --- */
typedef struct Node {
    int shapeIndex;
    struct Node *next;
} Node;

Node *head = NULL;
Node *tail = NULL;

void enqueueShape(int shapeIndex) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->shapeIndex = shapeIndex;
    newNode->next = NULL;
    if (!tail) head = tail = newNode;
    else { tail->next = newNode; tail = newNode; }
}

int dequeueShape() {
    if (!head) return rand() % 7;
    Node *temp = head;
    int val = temp->shapeIndex;
    head = head->next;
    if (!head) tail = NULL;
    free(temp);
    return val;
}

int peekNextShape() {
    if (head) return head->shapeIndex;
    return rand() % 7;
}

void freeQueue() {
    while (head) { Node *tmp = head; head = head->next; free(tmp); }
    tail = NULL;
}

/* --- SHAPE MEMORY HELPERS --- */
Shape current = {NULL, 0, 0, 0};

void safe_free_shape(Shape *s) {
    if (!s || !s->array) return;
    for (int i = 0; i < s->width; ++i) free(s->array[i]);
    free(s->array);
    s->array = NULL; s->width = 0;
}

static void alloc_shape_square(Shape *s, int w) {
    s->width = w;
    s->array = (char **)malloc(w * sizeof(char*));
    for (int i = 0; i < w; ++i) s->array[i] = (char*)calloc(w, sizeof(char));
}

Shape make_copy_from_constshape(int index) {
    Shape s = {NULL, 0, 0, 0};
    if (index < 0 || index > 6) return s;
    int w = ShapeWidths[index];
    alloc_shape_square(&s, w);
    const signed char *src = NULL;
    switch (index) {
        case 0: src = &S0_0[0][0]; break;
        case 1: src = &S1_0[0][0]; break;
        case 2: src = &S2_0[0][0]; break;
        case 3: src = &S3_0[0][0]; break;
        case 4: src = &S4_0[0][0]; break;
        case 5: src = &S5_0[0][0]; break;
        case 6: src = &S6_0[0][0]; break;
    }
    for (int i = 0; i < w; ++i)
        for (int j = 0; j < w; ++j)
            s.array[i][j] = (char)src[i * w + j];
    return s;
}

/* --- POSITION CHECKS & ROTATION --- */
int CheckPositionAt(char **array, int width, int row, int col) {
    for (int i = 0; i < width; ++i)
        for (int j = 0; j < width; ++j)
            if (array[i][j]) {
                int tr = row + i, tc = col + j;
                if (tc < 0 || tc >= COLS || tr >= ROWS) return FALSE;
                if (tr >= 0 && Table[tr][tc]) return FALSE;
            }
    return TRUE;
}

int CheckPositionBuf(char buf[4][4], int width, int row, int col) {
    for (int i = 0; i < width; ++i)
        for (int j = 0; j < width; ++j)
            if (buf[i][j]) {
                int tr = row + i, tc = col + j;
                if (tc < 0 || tc >= COLS || tr >= ROWS) return FALSE;
                if (tr >= 0 && Table[tr][tc]) return FALSE;
            }
    return TRUE;
}

void make_rotated_buffer(char buf[4][4], char **src, int width) {
    for (int i = 0; i < width; ++i)
        for (int j = 0; j < width; ++j)
            buf[i][j] = src[width - 1 - j][i];
}

void apply_rotated_to_current(char buf[4][4], int width) {
    for (int i = 0; i < width; ++i)
        for (int j = 0; j < width; ++j)
            current.array[i][j] = buf[i][j];
}

/* --- SCORE IO (kept) --- */
int load_all_scores(Player *players, int max) {
    FILE *f = fopen(SCORE_FILE, "r");
    if (!f) return 0;
    int count = 0;
    while (count < max && fscanf(f, "%49s %d", players[count].name, &players[count].score) == 2)
        count++;
    fclose(f);
    return count;
}

int compare_players_desc(const void *a, const void *b) {
    const Player *pa = (const Player *)a;
    const Player *pb = (const Player *)b;
    return pb->score - pa->score;
}

void save_all_scores(Player *players, int count) {
    if (count > 1) qsort(players, count, sizeof(Player), compare_players_desc);
    FILE *f = fopen(SCORE_FILE, "w");
    if (!f) return;
    for (int i = 0; i < count; ++i)
        fprintf(f, "%s %d\n", players[i].name, players[i].score);
    fclose(f);
}

void saveScore(const char *name, int score_v) {
    Player players[MAX_PLAYERS];
    int count = load_all_scores(players, MAX_PLAYERS);
    int found = 0;
    char loweredName[50];
    strncpy(loweredName, name, 49); loweredName[49] = '\0';
    for (int i = 0; loweredName[i]; i++) loweredName[i] = (char)tolower((unsigned char)loweredName[i]);

    for (int i = 0; i < count; ++i) {
        char storedLower[50]; strncpy(storedLower, players[i].name, 49); storedLower[49] = '\0';
        for (int j = 0; storedLower[j]; j++) storedLower[j] = (char)tolower((unsigned char)storedLower[j]);
        if (strcmp(storedLower, loweredName) == 0) {
            if (score_v > players[i].score) players[i].score = score_v;
            found = 1; break;
        }
    }
    if (!found && count < MAX_PLAYERS) {
        strncpy(players[count].name, name, 49); players[count].name[49] = '\0';
        players[count].score = score_v; count++;
    }
    if (count > 1) qsort(players, count, sizeof(Player), compare_players_desc);
    FILE *f = fopen(SCORE_FILE, "w");
    if (!f) return;
    for (int i = 0; i < count; ++i) fprintf(f, "%s %d\n", players[i].name, players[i].score);
    fclose(f);
}

void SetNewRandomShape(int shapeIndex) {
    Shape new_shape = make_copy_from_constshape(shapeIndex);
    new_shape.col = rand() % (COLS - new_shape.width + 1);
    new_shape.row = 0;
    safe_free_shape(&current);
    current = new_shape;
    if (!CheckPositionAt(current.array, current.width, current.row, current.col))
        GameOn = FALSE;
}



void RemoveFullRowsAndUpdateScore(void) {
    int lines = 0;

    for (int i = ROWS - 1; i >= 0; --i) {
        int full = 1;
        for (int j = 0; j < COLS; ++j)
            if (!Table[i][j]) { full = 0; break; }

        if (full) {
            // Flash the row white before removing
            for (int f = 0; f < 4; f++) {
                BeginDrawing();
                ClearBackground((Color){20, 20, 20, 255});
                DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                                       (Color){10, 10, 10, 255}, (Color){35, 35, 35, 255});

                // Draw grid
                drawGridAndCurrent();

                // Draw sidebar (HUD) so it doesn’t blink away
                int sidebarX = GRID_X + COLS * CELL + 70;
                int sidebarY = GRID_Y + 20;
                float t = GetTime();
                int r = (int)(128 + 127 * sin(t * 2.0));
                int g = (int)(128 + 127 * sin(t * 2.0 + 2.0));
                int b = (int)(128 + 127 * sin(t * 2.0 + 4.0));
                Color titleColor = (Color){r, g, b, 255};

                DrawText("TETRIS", GRID_X, 20, 32, titleColor);
                DrawText(TextFormat("Score: %d", score), sidebarX, sidebarY, 22, RAYWHITE);
                DrawText(TextFormat("Level: %d", level), sidebarX, sidebarY + 30, 18, LIGHTGRAY);
                DrawText(TextFormat("Gravity: %d ms", timer_ms), sidebarX, sidebarY + 50, 18, LIGHTGRAY);
                DrawText("Next:", sidebarX, sidebarY + 70, 20, DARKGRAY);
                drawNextPreview(sidebarX + 40, sidebarY + 95);

                // Flash white row
                for (int x = 0; x < COLS; x++) {
                    Rectangle rect = {
                        (float)(GRID_X + x * CELL),
                        (float)(GRID_Y + i * CELL),
                        (float)(CELL - 2),
                        (float)(CELL - 2)
                    };
                    DrawRectangleRec(rect, WHITE);
                }

                EndDrawing();
                WaitTime(0.05);
            }

            // Remove full line
            lines++;
            for (int k = i; k > 0; --k)
                memcpy(Table[k], Table[k - 1], COLS);
            memset(Table[0], 0, COLS);
            i = ROWS; // re-check from bottom
        }
    }

    // Update score + speed
    if (lines) {
        score += 100 * lines;
        timer_ms = (timer_ms > MIN_TIMER_MS + 20)
            ? timer_ms - 20
            : MIN_TIMER_MS;
    }
}


/* --- RENDER HELPERS (Raylib) --- */


Color cellColor(int val) {
    if (!val) return LIGHTGRAY;
    // simple palette by using value 1 -> blue; could be extended
    return SKYBLUE;
}

void drawGridAndCurrent(void) {
    // Buffer overlay
    char Buffer[ROWS][COLS]; memset(Buffer, 0, sizeof(Buffer));
    memcpy(Buffer, Table, sizeof(Table));
    if (current.array)
        for (int i = 0; i < current.width; ++i)
            for (int j = 0; j < current.width; ++j)
                if (current.array[i][j]) {
                    int r = current.row + i, c = current.col + j;
                    if (r >= 0 && r < ROWS && c >= 0 && c < COLS) Buffer[r][c] = 1;
                }

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            Rectangle rect = { (float)(GRID_X + c * CELL), (float)(GRID_Y + r * CELL), (float)(CELL - 2), (float)(CELL - 2)};
            DrawRectangleRec(rect, Buffer[r][c] ? SKYBLUE : (Color){40, 40, 40, 255});
            DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, (Color){60, 60, 60, 255});

        }
    }
}

void drawNextPreview(int px, int py) {
    int next = peekNextShape();
    int w = ShapeWidths[next];
    const signed char *src = NULL;
    switch (next) {
        case 0: src = &S0_0[0][0]; break;
        case 1: src = &S1_0[0][0]; break;
        case 2: src = &S2_0[0][0]; break;
        case 3: src = &S3_0[0][0]; break;
        case 4: src = &S4_0[0][0]; break;
        case 5: src = &S5_0[0][0]; break;
        case 6: src = &S6_0[0][0]; break;
    }
    // background box
    DrawRectangle(px - 6, py - 6, w * CELL + 12, w * CELL + 12, FOG);
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < w; ++j) {
            Rectangle r = { (float)(px + j * CELL), (float)(py + i * CELL), (float)(CELL - 2), (float)(CELL - 2) };
            DrawRectangleRec(r, src[i * w + j] ? ORANGE : LIGHTGRAY);
            DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, BLACK);
        }
    }
}

/* --- CONTROLS: reusing ManipulateCurrent --- */
int ManipulateCurrent(int action, int *shapeIndex) {
    if (!current.array) return 0;
    int changed = 0;
    if (action == 's') {
        if (CheckPositionAt(current.array, current.width, current.row + 1, current.col)) { current.row++; changed = 1; }
        else {
            WriteToTable(); RemoveFullRowsAndUpdateScore(); *shapeIndex = dequeueShape(); enqueueShape(rand() % 7); SetNewRandomShape(*shapeIndex); changed = 1;
        }
    } else if (action == 'a') {
        if (CheckPositionAt(current.array, current.width, current.row, current.col - 1)) { current.col--; changed = 1; }
    } else if (action == 'd') {
        if (CheckPositionAt(current.array, current.width, current.row, current.col + 1)) { current.col++; changed = 1; }
    } else if (action == 'w') {
        RotateCurrentIfPossible(); changed = 1;
    }
    return changed;
}
int showGameOverScreen(const char *name) {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        const char *msg = "GAME OVER";
        const char *sub = TextFormat("Final Score: %d", score);
        const char *opt1 = "Press R to Retry";
        const char *opt2 = "Press Q to Quit";

        int font1 = 72;
        int textW = MeasureText(msg, font1);
        float t = GetTime();
        Color pulse = (Color){255, (int)(64 + 64 * sin(t * 3)), (int)(64 + 64 * sin(t * 3)), 255};
        DrawText(msg, GetScreenWidth()/2 - textW/2, GetScreenHeight()/2 - 120, font1, pulse);


        DrawText(sub, GetScreenWidth()/2 - MeasureText(sub, 32)/2, GetScreenHeight()/2 - 40, 32, RAYWHITE);
        DrawText(opt1, GetScreenWidth()/2 - MeasureText(opt1, 20)/2, GetScreenHeight()/2 + 60, 20, LIGHTGRAY);
        DrawText(opt2, GetScreenWidth()/2 - MeasureText(opt2, 20)/2, GetScreenHeight()/2 + 90, 20, LIGHTGRAY);

        EndDrawing();

        if (IsKeyPressed(KEY_R)) return 1;  // retry
        if (IsKeyPressed(KEY_Q)) return 0;  // quit
    }
    return 0;
}
void showStartScreen(void) {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        const char *title = "TETRIS";
        const char *sub = "Press ENTER to start";
        const char *hint = "Press Q to quit";

        int font1 = 96;
        int font2 = 28;
        int w1 = MeasureText(title, font1);
        int w2 = MeasureText(sub, font2);

                // animate color using time
        float t = GetTime();
        int r = (int)(128 + 127 * sin(t * 2.0));   // red oscillates 0–255
        int g = (int)(128 + 127 * sin(t * 2.0 + 2.0)); // green with phase shift
        int b = (int)(128 + 127 * sin(t * 2.0 + 4.0)); // blue with phase shift
        Color flash = (Color){ r, g, b, 255 };

        // draw animated title
        DrawText(title, GetScreenWidth()/2 - w1/2, GetScreenHeight()/2 - 150, font1, flash);

        DrawText(sub, GetScreenWidth()/2 - w2/2, GetScreenHeight()/2 + 30, font2, LIGHTGRAY);
        DrawText(hint, GetScreenWidth()/2 - MeasureText(hint, 18)/2, GetScreenHeight()/2 + 80, 18, GRAY);

        EndDrawing();

        if (IsKeyPressed(KEY_ENTER)) break;
        if (IsKeyPressed(KEY_Q)) {
            CloseWindow();
            exit(0);  // user quit before starting
        }
    }
}
void WriteToTable(void) {
    for (int i = 0; i < current.width; ++i)
        for (int j = 0; j < current.width; ++j)
            if (current.array[i][j]) {
                int tr = current.row + i, tc = current.col + j;
                if (tr >= 0 && tr < ROWS && tc >= 0 && tc < COLS)
                    Table[tr][tc] = 1;
            }
}

void RotateCurrentIfPossible(void) {
    if (!current.array) return;
    int w = current.width;
    char buf[4][4] = {{0}};
    make_rotated_buffer(buf, current.array, w);
    int kicks[5][2] = {{0,0},{0,-1},{0,1},{-1,0},{0,-2}};
    for (int k = 0; k < 5; ++k) {
        int r = current.row + kicks[k][0];
        int c = current.col + kicks[k][1];
        if (CheckPositionBuf(buf, w, r, c)) {
            apply_rotated_to_current(buf, w);
            current.row = r;
            current.col = c;
            return;
        }
    }
}
void showScores(void) {
    FILE *f = fopen(SCORE_FILE, "r");
    if (!f) {
        printf("No high scores yet.\n");
        return;
    }

    printf("\n=== HIGH SCORES ===\n");
    char name[50];
    int s;
    int rank = 1;
    while (fscanf(f, "%49s %d", name, &s) == 2) {
        printf("%2d. %-20s %6d\n", rank++, name, s);
    }
    fclose(f);
    printf("===================\n");
}


void clearScores(void) {
    remove(SCORE_FILE);
    printf("All scores deleted.\n");
}
/*--- CRUD---*/
/* --- PLAYER CRUD SYSTEM --- */

int loadAllProfiles(PlayerProfile *arr, int max) {
    FILE *f = fopen(PLAYER_FILE, "r");
    if (!f) return 0;
    int count = 0;
    while (count < max && fscanf(f, "%49s %d", arr[count].name, &arr[count].bestScore) == 2)
        count++;
    fclose(f);
    return count;
}

void saveAllProfiles(PlayerProfile *arr, int count) {
    FILE *f = fopen(PLAYER_FILE, "w");
    if (!f) return;
    for (int i = 0; i < count; i++)
        fprintf(f, "%s %d\n", arr[i].name, arr[i].bestScore);
    fclose(f);
}

void updatePlayerScore(const char *name, int newScore) {
    PlayerProfile list[MAX_PROFILES];
    int count = loadAllProfiles(list, MAX_PROFILES);
    int found = 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0) {
            if (newScore > list[i].bestScore)
                list[i].bestScore = newScore;
            found = 1;
            break;
        }
    }

    if (!found && count < MAX_PROFILES) {
        strcpy(list[count].name, name);
        list[count].bestScore = newScore;
        count++;
    }

    saveAllProfiles(list, count);
}

void showAllPlayers() {
    PlayerProfile list[MAX_PROFILES];
    int count = loadAllProfiles(list, MAX_PROFILES);

    if (count == 0) {
        printf("No player profiles.\n");
        return;
    }

    printf("\n=== PLAYER PROFILES ===\n");
    for (int i = 0; i < count; i++)
        printf("%2d. %-20s Best Score: %d\n", i + 1, list[i].name, list[i].bestScore);
    printf("=======================\n");
}

void deletePlayerByName() {
    char target[50];
    printf("Enter player name to delete: ");
    scanf("%49s", target);

    PlayerProfile list[MAX_PROFILES];
    int count = loadAllProfiles(list, MAX_PROFILES);

    int newCount = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].name, target) != 0)
            list[newCount++] = list[i];
    }

    if (newCount == count)
        printf("Player not found.\n");
    else {
        saveAllProfiles(list, newCount);
        printf("Player deleted.\n");
    }
}


/* --- GAME PLAY (Raylib GUI loop) --- */
void playGame(const char *name) {
    
    

    // init window
    int winW = GRID_X + COLS * CELL + 320;
    int winH = GRID_Y + ROWS * CELL + 80;
    printf("Starting Tetris GUI...\n");
    fflush(stdout);

    InitWindow(winW, winH, "Tetris - Raylib GUI");
    SetTargetFPS(144);
    freeQueue();
    for (int i = 0; i < 3; ++i) enqueueShape(rand() % 7);
    int shapeIndex = dequeueShape();
    enqueueShape(rand() % 7);
    SetNewRandomShape(shapeIndex);
    showStartScreen();
    



    double nextFall = GetTime() + timer_ms / 1000.0;
    int needRedraw = 1;
    int paused = 0;
    double startTime = GetTime();  


    while (GameOn && !WindowShouldClose()) {
        // input
        if (IsKeyPressed(KEY_Q)) { GameOn = FALSE; break; }
        if (IsKeyPressed(KEY_P)) paused = !paused;
        if (!paused) {
    // --- Movement inputs ---
    if (IsKeyPressed(KEY_A) || IsKeyDown(KEY_LEFT)) {
        if (ManipulateCurrent('a', &shapeIndex)) needRedraw = 1;
    }
    if (IsKeyPressed(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        if (ManipulateCurrent('d', &shapeIndex)) needRedraw = 1;
    }
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        if (ManipulateCurrent('w', &shapeIndex)) needRedraw = 1;
    }

    // --- Hard drop (Space key) ---
    if (IsKeyPressed(KEY_SPACE)) {
        while (CheckPositionAt(current.array, current.width, current.row + 1, current.col)) {
            current.row++;
        }
        WriteToTable();
        RemoveFullRowsAndUpdateScore();
        shapeIndex = dequeueShape();
        enqueueShape(rand() % 7);
        SetNewRandomShape(shapeIndex);
        needRedraw = 1;
    }

    // --- Soft drop (holding S or DOWN) ---
    double now = GetTime();
    double fallDelay = timer_ms / 1000.0;

    // If player is holding S or Down, fall much faster
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        fallDelay = 0.06; // 60 ms per fall, smooth soft drop
    }

    if (now >= nextFall) {
        if (ManipulateCurrent('s', &shapeIndex)) needRedraw = 1;
        nextFall = now + fallDelay;
    }
}
        // --- Dynamic difficulty scaling ---
    double elapsed = GetTime() - startTime;

    // Level increases every 30 seconds OR every 1000 score points, whichever is higher
    int timeLevel = (int)(elapsed /30.0);
    int scoreLevel = score / 1000;
    level = (timeLevel > scoreLevel) ? timeLevel : scoreLevel;

    // Gravity decreases as level increases (faster drops)
    int new_timer = BASE_TIMER_MS - (level * 15);
    if (new_timer < MIN_TIMER_MS) new_timer = MIN_TIMER_MS;
    timer_ms = new_timer;



        // drawing
    BeginDrawing();
    ClearBackground((Color){ 20, 20, 20, 255 });  // Dark gray background
    // Soft vignette background for visual depth
    DrawRectangleGradientV(0, 0, winW, winH, (Color){10, 10, 10, 255}, (Color){35, 35, 35, 255});  // consistent dark theme

if (!paused) {
    // --- Normal game rendering ---
    int sidebarX = GRID_X + COLS * CELL + 70;
    int sidebarY = GRID_Y + 20;

    float t = GetTime();
    int r = (int)(128 + 127 * sin(t * 2.0));
    int g = (int)(128 + 127 * sin(t * 2.0 + 2.0));
    int b = (int)(128 + 127 * sin(t * 2.0 + 4.0));
    Color titleColor = (Color){r, g, b, 255};
    DrawText("TETRIS", GRID_X, 20, 32, titleColor);
    DrawText(TextFormat("Score: %d", score), sidebarX, sidebarY, 22, RAYWHITE);
    DrawText(TextFormat("Level: %d", level), sidebarX, sidebarY + 30, 18, LIGHTGRAY);
    DrawText(TextFormat("Gravity: %d ms", timer_ms), sidebarX, sidebarY + 50, 18, LIGHTGRAY);


    DrawText("Next:", sidebarX, sidebarY + 70, 20, DARKGRAY);
    drawNextPreview(sidebarX + 40, sidebarY + 95);

    int controlBoxX = sidebarX - 15;
    int controlBoxY = sidebarY + 190;
    int controlBoxW = 250;
    int controlBoxH = 160;

    DrawRectangle(controlBoxX, controlBoxY, controlBoxW, controlBoxH, Fade((Color){50,50,50,255}, 0.6f));
    DrawRectangleLines(controlBoxX, controlBoxY, controlBoxW, controlBoxH, (Color){90,90,90,255});

    int controlTextY = controlBoxY + 10;
    DrawText("Controls:", sidebarX, sidebarY + 210, 16, RAYWHITE);
    DrawText("A/D or right/left arrows : Move", sidebarX, sidebarY + 230, 14, LIGHTGRAY);
    DrawText("S or down arrow : Soft Drop", sidebarX, sidebarY + 250, 14, LIGHTGRAY);
    DrawText("Space : Hard Drop", sidebarX, sidebarY + 270, 14, LIGHTGRAY);
    DrawText("W or up arrow : Rotate", sidebarX, sidebarY + 290, 14, LIGHTGRAY);
    DrawText("P: Pause, Q: Quit", sidebarX, sidebarY + 310, 14, LIGHTGRAY);


    drawGridAndCurrent();
}
else {
    // --- Pause overlay ---
    DrawRectangle(0, 0, winW, winH, Fade(BLACK, 0.6f));
    const char *pauseText = "PAUSED";
    int fontSize = 64;
    int textWidth = MeasureText(pauseText, fontSize);
    int x = winW/2 - textWidth/2;
    int y = winH/2 - fontSize/2;
    DrawText(pauseText, x, y, fontSize, RAYWHITE);

    const char *resumeHint = "Press P to resume";
    int hintSize = 22;
    int hintWidth = MeasureText(resumeHint, hintSize);
    DrawText(resumeHint, winW/2 - hintWidth/2, y + fontSize + 15, hintSize, LIGHTGRAY);
}

EndDrawing();

        if (needRedraw) needRedraw = 0;
    }

    // --- End of game ---
    safe_free_shape(&current);
    saveScore(name, score);
    updatePlayerScore(name, score);
    freeQueue();

    // If player lost naturally (not by pressing Q)
    if (!WindowShouldClose()) {
        int choice = showGameOverScreen(name);
        if (choice == 1) {
            // Retry game
            memset(Table, 0, sizeof(Table));
            score = 0;
            timer_ms = BASE_TIMER_MS;
            GameOn = TRUE;
            playGame(name);  // restart without going to console
            return;
        }
    }

    // Quit back to console
    CloseWindow();

    }

/* --- MAIN MENU (keeps console menu intact) --- */
static void flush_stdin(void) { int ch; while ((ch = getchar()) != '\n' && ch != EOF) {} }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    srand((unsigned)time(NULL));

    int choice = 0; char name[50];
    printf("Enter your name: ");
    if (scanf("%49s", name) != 1) return 1;
    flush_stdin();

    do {
        printf("\n==== TETRIS MENU ====\n");
        printf("1. New Game\n");
        printf("2. View High Scores\n");
        printf("3. Clear High Scores\n");
        printf("4. Quit\n");
        printf("5. Players\n");
        printf("Choose an option: ");
        if (scanf("%d", &choice) != 1) { flush_stdin(); printf("Invalid choice.\n"); continue; }
        flush_stdin();

        switch (choice) {
            case 1: memset(Table, 0, sizeof(Table)); score = 0; timer_ms = BASE_TIMER_MS; GameOn = TRUE; playGame(name); break;
            case 2: showScores(); break;
            case 3: clearScores(); break;
            case 4: printf("Goodbye, %s!\n", name); break;
            case 5: {
    int pc = 0;
    do {
        printf("\n--- PLAYER MANAGER ---\n");
        printf("1. Show all players\n");
        printf("2. Add new player\n");
        printf("3. Delete a player\n");
        printf("4. Back\n");
        printf("Choose: ");
        scanf("%d", &pc);
        flush_stdin();

        if (pc == 1) showAllPlayers();
        else if (pc == 2) {
            char newName[50];
            printf("Enter new player name: ");
            scanf("%49s", newName);
            updatePlayerScore(newName, 0);
        }
        else if (pc == 3) deletePlayerByName();
    } while (pc != 4);
    break;
}

            default: printf("Invalid choice.\n");
        }
    } while (choice != 4);

    return 0;
}