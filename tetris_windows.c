#include "raylib.h"
#define FOG (Color){ 100, 100, 100, 100 } // semi-transparent gray fog
#include<math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
int CELL = 26;

#define ROWS 20
#define COLS 15
#define TRUE 1
#define FALSE 0
#define SCORE_FILE "scores.txt"
#define MAX_PLAYERS 200
#define MIN_TIMER_MS 50
#define BASE_TIMER_MS 400
typedef struct Profile {
    char name[50];
    int timesPlayed;
    int bestScore;
    int totalTime;
    struct Profile *next;
} Profile;
Profile *profileHead = NULL;
Profile *currentPlayer = NULL;


/* Forward prototypes (kept mostly same names so logic remains intact) */
void showScores(void);
void clearScores(void);
void saveScore(const char *name, int score_v);
void drawGridAndCurrent(void);
void drawNextPreview(int px, int py);
void WriteToTable(void);
void RotateCurrentIfPossible(void);
void saveProfiles(void);
int themedInputBox(const char *prompt);
void addProfile_GUI(void);
void searchProfile_GUI(void);
void updateProfile_GUI(void);
void deleteProfile_GUI(void);
void listProfiles_GUI(void);
int askSaveDataScreen(int finalScore);
int showGameOverScreen(const char *playerName);
void resetGameState(void);
void playGame(Profile *player);
void loadProfiles();
Profile* searchProfileByName(const char *name);
void playerManagerMenu_GUI(void);
void drawSidebar(Profile *player, int elapsed);


int GRID_X = 40;
int GRID_Y = 80;


/* --- Globals reused from your console version --- */
char Table[ROWS][COLS];
int score = 0;
int GameOn = TRUE;
int timer_ms = BASE_TIMER_MS;
int level = 0;
int selectedProfileIndex = -1;



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
    int open;
    int index;
    int x, y;
} PopupMenu;

PopupMenu popup = {0, -1, 0, 0};


/* --- LINKED LIST --- */
typedef struct Node {
    int shapeIndex;
    struct Node *next;
} Node;

Node *head = NULL;
Node *tail = NULL;
Color neonPulse() {
    float t = GetTime();
    int r = (int)(128 + 127 * sin(t * 2.0));
    int g = (int)(128 + 127 * sin(t * 2.0 + 2.0));
    int b = (int)(128 + 127 * sin(t * 2.0 + 4.0));
    return (Color){r, g, b, 255};
}




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
                int sidebarX = GRID_X + COLS * CELL + (CELL * 2);
                int sidebarY = GRID_Y;

                float t = GetTime();
                int r = (int)(128 + 127 * sin(t * 2.0));
                int g = (int)(128 + 127 * sin(t * 2.0 + 2.0));
                int b = (int)(128 + 127 * sin(t * 2.0 + 4.0));
                Color titleColor = (Color){r, g, b, 255};

                drawSidebar(currentPlayer, 0);

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




/* --- MAIN MENU (keeps console menu intact) --- */
static void flush_stdin(void) { int ch; while ((ch = getchar()) != '\n' && ch != EOF) {} }
/* ====================== PLAYER PROFILE CRUD ====================== */

#define PROFILE_DB "playerdb.txt"




/* Load all profiles from file */
void loadProfiles() {
    FILE *f = fopen(PROFILE_DB, "r");
    if (!f) return;

    Profile temp;
    temp.totalTime = 0; // if old format
    while (fscanf(f, "%49s %d %d %d", temp.name, &temp.timesPlayed, &temp.bestScore, &temp.totalTime) == 4)
    

{
        Profile *node = (Profile *)malloc(sizeof(Profile));
        *node = temp;
        node->next = profileHead;
        profileHead = node;
    }
    fclose(f);
}

/* Save all profiles to file */
void saveProfiles() {
    FILE *f = fopen(PROFILE_DB, "w");
    if (!f) return;

    Profile *p = profileHead;
    while (p) {
        fprintf(f, "%s %d %d %d\n", p->name, p->timesPlayed, p->bestScore, p->totalTime);


        p = p->next;
    }
    fclose(f);
}



/* Search a profile */
Profile* searchProfileByName(const char *name) {
    Profile *p = profileHead;
    while (p) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->next;
    }
    return NULL;
}


typedef struct {
    Rectangle rect;
    const char *label;
} Button;

int ButtonClicked(Button b) {
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, b.rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}




void DrawButtonTheme(Button b) {
    Vector2 m = GetMousePosition();
    int hover = CheckCollisionPointRec(m, b.rect);

    Color border = hover ? neonPulse() : (Color){90,90,90,255};
    Color fill   = hover ? (Color){50,50,50,255} : (Color){30,30,30,255};

    DrawRectangleRec(b.rect, fill);
    DrawRectangleLinesEx(b.rect, 2, border);

    int w = MeasureText(b.label, 22);
    DrawText(b.label, b.rect.x + b.rect.width/2 - w/2, b.rect.y + 8, 22, RAYWHITE);
}

char inputBuffer[50];

int themedInputBox(const char *prompt) {
    SetExitKey(KEY_NULL);
    memset(inputBuffer, 0, sizeof(inputBuffer));

    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();

        int pw = MeasureText(prompt, 34);
        DrawText(prompt, w/2 - pw/2, h/3 - 40, 34, pulse);

        Rectangle box = {
            w * 0.20f,
            h * 0.45f,
            w * 0.60f,
            h * 0.08f
        };

        DrawRectangleRec(box, (Color){40,40,40,255});
        DrawRectangleLinesEx(box, 2, pulse);

        DrawText(inputBuffer, box.x + 10, box.y + 12, 26, RAYWHITE);

        const char *hint = "ENTER = OK   ESC = Back";
        int hw = MeasureText(hint, 20);
        DrawText(hint, w/2 - hw/2, box.y + box.height * 0.65f, 20, LIGHTGRAY);


        EndDrawing();

        if (IsKeyPressed(KEY_BACKSPACE) && strlen(inputBuffer) > 0)
            inputBuffer[strlen(inputBuffer)-1] = 0;

        int c = GetCharPressed();
        if (c >= 32 && c <= 126 && strlen(inputBuffer) < 49)
            inputBuffer[strlen(inputBuffer)] = (char)c;

        if (IsKeyPressed(KEY_ENTER)) return 1;
        if (IsKeyPressed(KEY_ESCAPE)) return 0;
    }
}
Profile* getPlayerFromGUI() {
    SetExitKey(KEY_NULL);
    while (!WindowShouldClose()) {
        // Ask for name
        if (!themedInputBox("Enter Player Name:")) return NULL;

        if (strlen(inputBuffer) == 0)
            continue;

        // Existing player?
        Profile *p = searchProfileByName(inputBuffer);
        if (p) return p;

        // New profile
        Profile *np = malloc(sizeof(Profile));
        strncpy(np->name, inputBuffer, 49);
        np->name[49] = 0;
        np->timesPlayed = 0;
        np->bestScore = 0;
        np->next = profileHead;
        profileHead = np;
        np->totalTime = 0;

        saveProfiles();
        currentPlayer = np;


        return np;
    }
    return NULL;
}


int DrawBackButton() {
    Button backBtn = { { 40, GetScreenHeight() - 80, 160, 40 }, "Back" };

    DrawButtonTheme(backBtn);

    if (ButtonClicked(backBtn)) return 1;
    return 0;
}


void addProfile_GUI() {
    SetExitKey(KEY_NULL);
    while (1) {
    if (!themedInputBox("Enter Player Name:")) return;
    if (strlen(inputBuffer) == 0) return;
    break;
}
    Profile *p = malloc(sizeof(Profile));

    strncpy(p->name, inputBuffer, 49);
    p->name[49] = 0;

    p->timesPlayed = 0;
    p->bestScore = 0;
    p->totalTime = 0;

    p->next = profileHead;
    profileHead = p;

    saveProfiles();
}
void searchProfile_GUI() {
    SetExitKey(KEY_NULL);
    if (!themedInputBox("Search Player:")) return;


    Profile *p = searchProfileByName(inputBuffer);

    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();

        // Title
        const char *title = "SEARCH RESULT";
        int tw = MeasureText(title, 36);
        DrawText(title, w/2 - tw/2, 60, 36, pulse);

        float boxW = w * 0.50f;
        float boxH = h * 0.22f;
        float boxX = w/2 - boxW/2;
        float boxY = h/2 - boxH/2;

        DrawRectangle(boxX, boxY, boxW, boxH, (Color){40,40,40,180});
        DrawRectangleLines(boxX, boxY, boxW, boxH, pulse);

        float tx = boxX + 20;
        float ty = boxY + 20;

        if (!p) {
            DrawText("NOT FOUND", tx, ty, 32, RED);
        } else {
            DrawText(TextFormat("Name: %s", p->name), tx, ty, 26, RAYWHITE); 
            ty += 30;
            DrawText(TextFormat("Times Played: %d", p->timesPlayed), tx, ty, 22, LIGHTGRAY);
            ty += 30;
            DrawText(TextFormat("Best Score: %d", p->bestScore), tx, ty, 22, LIGHTGRAY);
            ty += 30;
            DrawText(TextFormat("Total Time: %02d:%02d", p->totalTime/60, p->totalTime%60), tx, ty, 22, LIGHTGRAY);

        }


        if (DrawBackButton()) return;

        EndDrawing();
    }
}

void updateProfile_GUI() {
    SetExitKey(KEY_NULL);
    if (!themedInputBox("Enter Old Name:")) return;
    Profile *p = searchProfileByName(inputBuffer);
    if (!p) return;


    if (!themedInputBox("Enter New Name:")) return;

    strncpy(p->name, inputBuffer, 49);
    p->name[49] = 0;

    saveProfiles();
     while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        Color pulse = neonPulse();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),
                               (Color){10,10,10,255},(Color){40,40,40,255});

        float boxW = w * 0.40f;
        float boxH = h * 0.15f;
        float boxX = w/2 - boxW/2;
        float boxY = h/2 - boxH/2;

        DrawRectangle(boxX, boxY, boxW, boxH, (Color){40,40,40,180});
        DrawRectangleLines(boxX, boxY, boxW, boxH, pulse);

        float tx = boxX + 20;
        float ty = boxY + 30;
        DrawText("Profile Updated!", tx, ty, 32, RAYWHITE);


        if (DrawBackButton()) return;

        EndDrawing();
    }
}

void deleteProfile_GUI() {
    SetExitKey(KEY_NULL);
    if (!themedInputBox("Delete Player:")) return;


    Profile *prev = NULL;
    Profile *curr = profileHead;

    int deleted = 0;  // <-- THIS FIXES EVERYTHING

    while (curr) {
        if (strcmp(curr->name, inputBuffer) == 0) {
            if (!prev) profileHead = curr->next;
            else prev->next = curr->next;

            free(curr);
            saveProfiles();
            deleted = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        Color pulse = neonPulse();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),
                               (Color){10,10,10,255},(Color){40,40,40,255});

        float boxW = w * 0.40f;
        float boxH = h * 0.15f;
        float boxX = w/2 - boxW/2;
        float boxY = h/2 - boxH/2;

        DrawRectangle(boxX, boxY, boxW, boxH, (Color){40,40,40,180});
        DrawRectangleLines(boxX, boxY, boxW, boxH, pulse);

        float tx = boxX + 20;
        float ty = boxY + 30;

        if (deleted)
            DrawText("Profile Deleted!", tx, ty, 32, RED);
        else
            DrawText("Profile Not Found", tx, ty, 32, RAYWHITE);


                if (DrawBackButton()) return;

                EndDrawing();
            }
        }
int compareProfilesDesc(const void *a, const void *b) {
    Profile *pa = *(Profile**)a;
    Profile *pb = *(Profile**)b;
    return pb->bestScore - pa->bestScore;
}

void deleteAllProfiles_GUI() {
    SetExitKey(KEY_NULL);

    // Confirm screen
    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        Color pulse = neonPulse();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        const char *msg = "Delete ALL profiles?";
        const char *warn = "This cannot be undone.";
        const char *optYes = "Y - Confirm";
        const char *optNo  = "N / Back";

        DrawText(msg,  w/2 - MeasureText(msg,  36)/2, h/3,     36, RED);
        DrawText(warn, w/2 - MeasureText(warn, 24)/2, h/3 +40, 24, LIGHTGRAY);
        DrawText(optYes,w/2 - MeasureText(optYes,24)/2, h/3+120,24, GREEN);
        DrawText(optNo, w/2 - MeasureText(optNo, 24)/2, h/3+160,24, RAYWHITE);

        EndDrawing();

        if (IsKeyPressed(KEY_Y)) break;
        if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) return;
    }

    // DELETE EVERYTHING
    Profile *p = profileHead;
    while (p) {
        Profile *temp = p;
        p = p->next;
        free(temp);
    }
    profileHead = NULL;

    // wipe file
    remove(PROFILE_DB);

    // Success message
    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        Color pulse = neonPulse();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        const char *msg = "All Profiles Deleted!";
        DrawText(msg, w/2 - MeasureText(msg, 32)/2, h/2 - 20, 32, RED);

        if (DrawBackButton()) return;

        EndDrawing();
    }
}

void drawProfilePopup(Profile *pr) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    int boxW = 260;
    int boxH = 140;

    int x = popup.x;
    int y = popup.y;

    // keep popup inside screen
    if (x + boxW > w) x = w - boxW - 10;
    if (y + boxH > h) y = h - boxH - 10;

    Rectangle box = { x, y, boxW, boxH };

    DrawRectangleRec(box, (Color){30,30,30,240});
    DrawRectangleLinesEx(box, 2, neonPulse());

    DrawText("Profile Options", x + 10, y + 10, 22, RAYWHITE);

    Button btnEdit = { { x + 20, y + 50, boxW - 40, 30 }, "Edit Name" };
    Button btnDelete = { { x + 20, y + 90, boxW - 40, 30 }, "Delete Profile" };

    DrawButtonTheme(btnEdit);
    DrawButtonTheme(btnDelete);

    if (ButtonClicked(btnEdit)) {
        popup.open = 0;

        // edit name
        if (themedInputBox("Enter New Name:")) {
            strncpy(pr->name, inputBuffer, 49);
            pr->name[49] = 0;
            saveProfiles();
        }
    }

    if (ButtonClicked(btnDelete)) {
        popup.open = 0;

        // delete this profile properly
        Profile *prev = NULL, *cur = profileHead;
        while (cur) {
            if (cur == pr) {
                if (prev) prev->next = cur->next;
                else profileHead = cur->next;
                free(cur);
                saveProfiles();
                popup.index = -999;
                return;
            }
            prev = cur;
            cur = cur->next;
        }
    }
}


void listProfiles_GUI() {
    SetExitKey(KEY_NULL);
    int scroll = 0;

    // Count profiles
    int count = 0;
    Profile *p = profileHead;
    while (p) { count++; p = p->next; }

    if (count == 0) {
        while (1) {
            BeginDrawing();
            ClearBackground((Color){20,20,20,255});
            DrawText("No Profiles Found", 50, 50, 28, RED);
            if (DrawBackButton()) return;
            EndDrawing();
        }
    }

    // Convert LL → array
    Profile **arr = malloc(sizeof(Profile*) * count);
    p = profileHead;
    for (int i = 0; i < count; i++) { arr[i] = p; p = p->next; }

    // Sort by best score
    qsort(arr, count, sizeof(Profile*), compareProfilesDesc);

    while (1) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        int tableX = w * 0.08f;
        int tableW = w * 0.84f;
        int rowH   = 44;

        // dynamic columns
        int colNameW  = tableW * 0.32f;
        int colPlayW  = tableW * 0.16f;
        int colBestW  = tableW * 0.22f;
        int colTimeW  = tableW * 0.30f;

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();
        DrawText("PLAYER PROFILES", w/2 - MeasureText("PLAYER PROFILES", 42)/2, 30, 42, pulse);

        int y = 120 + scroll;

        // HEADER
        DrawRectangle(tableX, y, tableW, rowH, (Color){35,35,35,255});
        DrawRectangleLines(tableX, y, tableW, rowH, pulse);

        DrawText("Name",        tableX + 10,              y + 10, 22, RAYWHITE);
        DrawText("Played",      tableX + colNameW + 10,   y + 10, 22, RAYWHITE);
        DrawText("Best Score",  tableX + colNameW + colPlayW + 10, y + 10, 22, RAYWHITE);
        DrawText("Total Time",  tableX + colNameW + colPlayW + colBestW + 10, y + 10, 22, RAYWHITE);

        y += rowH;

        Vector2 m = GetMousePosition();

        for (int i = 0; i < count; i++) {
            Profile *pr = arr[i];

            Rectangle rowRect = { tableX, y, tableW, rowH };

            // hover highlight
            int hover = CheckCollisionPointRec(m, rowRect);
            int selected = (i == selectedProfileIndex);

            Color bg = (i % 2 == 0) ? (Color){30,30,30,220} : (Color){40,40,40,220};
            if (hover) bg = (Color){60,60,60,240};
            if (selected) bg = (Color){80,20,20,240};

            DrawRectangleRec(rowRect, bg);
            DrawRectangleLines(tableX, y, tableW, rowH, (Color){70,70,70,255});

            // TEXT
            DrawText(pr->name, tableX + 10, y + 12, 20, RAYWHITE);
            DrawText(TextFormat("%d", pr->timesPlayed),
                    tableX + colNameW + 10, y + 12, 20, LIGHTGRAY);
            DrawText(TextFormat("%d", pr->bestScore),
                    tableX + colNameW + colPlayW + 10, y + 12, 20, LIGHTGRAY);
            DrawText(TextFormat("%02d:%02d", pr->totalTime/60, pr->totalTime%60),
                    tableX + colNameW + colPlayW + colBestW + 10, y + 12, 20, LIGHTGRAY);

                        // Left click = select row
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selectedProfileIndex = i;
            }

            // Right click = open popup menu
            if (hover && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                popup.open = 1;
                popup.index = i;
                popup.x = m.x;
                popup.y = m.y;
            }



            y += rowH;
        }

        scroll += GetMouseWheelMove() * -20;
        if (popup.open && popup.index >= 0 && popup.index < count) {
            drawProfilePopup(arr[popup.index]);
        }

        // check if delete happened
        if (popup.index == -999) {
            popup.index = -1;

            // free old arr
            free(arr);

            // rebuild count
            count = 0;
            p = profileHead;
            while (p) { count++; p = p->next; }

            // if empty, exit back
            if (count == 0) return;

            // rebuild arr
            arr = malloc(sizeof(Profile*) * count);
            p = profileHead;
            for (int i = 0; i < count; i++) {
                arr[i] = p;
                p = p->next;
            }

            // re-sort
            qsort(arr, count, sizeof(Profile*), compareProfilesDesc);
        }


        if (DrawBackButton()) break;

        EndDrawing();
    }
    popup.open = 0;
    popup.index = -1;


    free(arr);
}


void showScores_GUI() {
    Player p[MAX_PLAYERS];
    int count = load_all_scores(p, MAX_PLAYERS);

    SetExitKey(KEY_NULL);
    int scroll = 0;

    while (!WindowShouldClose()) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();
        const char *title = "HIGH SCORES";
        int tw = MeasureText(title, 36);
        DrawText(title, w/2 - tw/2, 40, 36, pulse);

        int y = 120 + scroll;

        for (int i = 0; i < count; ++i) {
            DrawText(
                TextFormat("%d) %s   -   %d", i+1, p[i].name, p[i].score),
                w/2 - 250, y, 24, RAYWHITE
            );
            y += 35;
        }

        scroll += GetMouseWheelMove() * -20;

        if (DrawBackButton()) return;

        EndDrawing();
    }
}

void playerManagerMenu_GUI(void) {
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);

    while (!WindowShouldClose()) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        float bw = w * 0.30f;
        float bh = h * 0.07f;
        float cx = w * 0.50f - bw/2;

        Button addBtn       = {{ cx, h * 0.30f, bw, bh }, "Add Profile" };
        Button searchBtn    = {{ cx, h * 0.40f, bw, bh }, "Search Profile" };
        Button updateBtn    = {{ cx, h * 0.50f, bw, bh }, "Update Profile" };
        Button deleteBtn    = {{ cx, h * 0.60f, bw, bh }, "Delete Profile" };
        Button deleteAllBtn = {{ cx, h * 0.70f, bw, bh }, "Delete ALL Profiles" };
        Button listBtn      = {{ cx, h * 0.80f, bw, bh }, "List All Profiles" };
        Button backBtn      = {{ cx, h * 0.90f, bw, bh }, "Back" };


        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();
        const char *title = "PLAYER MANAGER";
        int tw = MeasureText(title, 38);
        DrawText(title, w/2 - tw/2, 60, 38, pulse);

        DrawButtonTheme(addBtn);
        DrawButtonTheme(searchBtn);
        DrawButtonTheme(updateBtn);
        DrawButtonTheme(deleteBtn);
        DrawButtonTheme(deleteAllBtn);
        DrawButtonTheme(listBtn);
        DrawButtonTheme(backBtn);

        EndDrawing();

        if (ButtonClicked(addBtn))    addProfile_GUI();
        if (ButtonClicked(searchBtn)) searchProfile_GUI();
        if (ButtonClicked(updateBtn)) updateProfile_GUI();
        if (ButtonClicked(deleteBtn)) deleteProfile_GUI();
        if (ButtonClicked(deleteAllBtn)) deleteAllProfiles_GUI();
        if (ButtonClicked(listBtn))   listProfiles_GUI();
        if (ButtonClicked(backBtn))   return;

    }
}
int mainMenu_GUI(Profile *player) {
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);

    while (!WindowShouldClose()) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        float bw = w * 0.30f;
        float bh = h * 0.08f;
        float cx = w * 0.50f - bw/2;

        Button newGame = {{cx, h*0.34f, bw, bh}, "New Game"};
        Button manage  = {{cx, h*0.48f, bw, bh}, "Player Manager"};
        Button quit    = {{cx, h*0.62f, bw, bh}, "Quit"};



        BeginDrawing();
        ClearBackground((Color){15,15,15,255});
        DrawRectangleGradientV(0,0,w,h,(Color){10,10,10,255},(Color){40,40,40,255});

        Color pulse = neonPulse();
        const char *title = "TETRIS MENU";
        int tw = MeasureText(title, 42);
        DrawText(title, w/2 - tw/2, 60, 42, pulse);
        DrawButtonTheme(newGame);
        DrawButtonTheme(manage);
        DrawButtonTheme(quit);



        EndDrawing();
        if (ButtonClicked(newGame)) return 1;
        if (ButtonClicked(manage))  return 2;
        if (ButtonClicked(quit))    return 3;


    }

    return 3;
}

int askSaveDataScreen(int finalScore) {
    SetExitKey(KEY_NULL);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        const char *msg = "Save this game data?";
        const char *scoreMsg = TextFormat("Score: %d", finalScore);
        const char *yesMsg = "Y - Save";
        const char *noMsg  = "N / ESC - Skip";

        DrawText(msg, GetScreenWidth()/2 - MeasureText(msg, 40)/2, 150, 40, RAYWHITE);
        DrawText(scoreMsg, GetScreenWidth()/2 - MeasureText(scoreMsg, 30)/2, 220, 30, LIGHTGRAY);
        DrawText(yesMsg, GetScreenWidth()/2 - MeasureText(yesMsg, 24)/2, 310, 24, GREEN);
        DrawText(noMsg,  GetScreenWidth()/2 - MeasureText(noMsg,  24)/2, 350, 24, RED);

        EndDrawing();

        if (IsKeyPressed(KEY_Y)) return 1;
        if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) return 0;
    }
    return 0;
}
int showGameOverScreen(const char *playerName) {
    SetExitKey(KEY_NULL);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        const char *msg  = "GAME OVER";
        const char *sub1 = TextFormat("Player: %s", playerName);
        const char *sub2 = TextFormat("Final Score: %d", score);
        const char *opt1 = "R - Retry";
        const char *opt2 = "Q / ESC - Quit to Console";
        const char *opt3 = "E - Return to Main Menu";
        int font1 = 72;
        int textW = MeasureText(msg, font1);
        float t = GetTime();
        Color pulse = (Color){255, (int)(64 + 64 * sin(t * 3)), (int)(64 + 64 * sin(t * 3)), 255};

        DrawText(msg, GetScreenWidth()/2 - textW/2, GetScreenHeight()/2 - 150, font1, pulse);
        DrawText(sub1, GetScreenWidth()/2 - MeasureText(sub1, 26)/2, GetScreenHeight()/2 - 40, 26, RAYWHITE);
        DrawText(sub2, GetScreenWidth()/2 - MeasureText(sub2, 26)/2, GetScreenHeight()/2,     26, LIGHTGRAY);
        DrawText(opt1, GetScreenWidth()/2 - MeasureText(opt1, 20)/2, GetScreenHeight()/2 + 80, 20, LIGHTGRAY);
        DrawText(opt2, GetScreenWidth()/2 - MeasureText(opt2, 20)/2, GetScreenHeight()/2 + 110,20, LIGHTGRAY);
        DrawText(opt3, GetScreenWidth()/2 - MeasureText(opt3, 20)/2, GetScreenHeight()/2 + 140, 20, LIGHTGRAY);


        EndDrawing();

        if (IsKeyPressed(KEY_R)) return 1;
        if (IsKeyPressed(KEY_E)) return 2;
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) return 0;
    }
    return 0;
}
void resetGameState(void) {
    memset(Table, 0, sizeof(Table));
    score    = 0;
    level    = 0;
    timer_ms = BASE_TIMER_MS;
    GameOn   = TRUE;

    safe_free_shape(&current);
    freeQueue();
    for (int i = 0; i < 3; ++i) {
        enqueueShape(rand() % 7);
    }
    int shapeIndex = dequeueShape();
    enqueueShape(rand() % 7);
    SetNewRandomShape(shapeIndex);
}
void drawSidebar(Profile *player, int elapsed) {
    float winW = GetScreenWidth();
    float winH = GetScreenHeight();

    float sidebarX = winW * 0.70f;
    float sidebarY = winH * 0.10f;

    float y = sidebarY;

    DrawText(TextFormat("Player: %s", player->name), sidebarX, y, 18, LIGHTGRAY);
    y += 40;

    DrawText(TextFormat("Score: %d", score), sidebarX, y, 22, RAYWHITE);
    y += 40;

    DrawText(TextFormat("Level: %d", level), sidebarX, y, 18, LIGHTGRAY);
    y += 40;

    DrawText(TextFormat("Gravity: %d ms", timer_ms), sidebarX, y, 18, LIGHTGRAY);
    y += 40;

    DrawText(TextFormat("Time: %02d:%02d", elapsed/60, elapsed%60), sidebarX, y, 18, LIGHTGRAY);
    y += 40;

    DrawText("Next:", sidebarX, y, 20, DARKGRAY);
    y += 30;

    int nextShape = peekNextShape();
    int nW = ShapeWidths[nextShape];

    drawNextPreview(sidebarX + CELL, y);
}


void playGame(Profile *player) {
    int winW, winH;

    resetGameState();

    double nextFall = GetTime() + timer_ms / 1000.0;
    double startTime = GetTime();
    int paused = 0;
    int shapeIndex = 0;

    while (GameOn && !WindowShouldClose()) {
        // Quit mid-game
        if (IsKeyPressed(KEY_Q)) {
            GameOn = FALSE;
            break;
        }

        if (IsKeyPressed(KEY_P)) {
            paused = !paused;
        }

        if (!paused) {
            // Movement
            if (IsKeyPressed(KEY_A) || IsKeyDown(KEY_LEFT)) {
                if (ManipulateCurrent('a', &shapeIndex)) {}
            }
            if (IsKeyPressed(KEY_D) || IsKeyDown(KEY_RIGHT)) {
                if (ManipulateCurrent('d', &shapeIndex)) {}
            }
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                if (ManipulateCurrent('w', &shapeIndex)) {}
            }

            // Hard drop
            if (IsKeyPressed(KEY_SPACE)) {
                while (CheckPositionAt(current.array, current.width, current.row + 1, current.col)) {
                    current.row++;
                }
                WriteToTable();
                RemoveFullRowsAndUpdateScore();
                shapeIndex = dequeueShape();
                enqueueShape(rand() % 7);
                SetNewRandomShape(shapeIndex);
            }

            // Gravity
            double now = GetTime();
            double fallDelay = timer_ms / 1000.0;

            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
                fallDelay = 0.06;
            }

            if (now >= nextFall) {
                if (!CheckPositionAt(current.array, current.width, current.row + 1, current.col)) {
                    // lock piece
                    WriteToTable();
                    RemoveFullRowsAndUpdateScore();
                    shapeIndex = dequeueShape();
                    enqueueShape(rand() % 7);
                    SetNewRandomShape(shapeIndex);
                } else {
                    current.row++;
                }
                nextFall = now + fallDelay;
            }

            // Dynamic difficulty
            double elapsed = GetTime() - startTime;
            int timeLevel  = (int)(elapsed / 30.0);
            int scoreLevel = score / 1000;
            level = (timeLevel > scoreLevel) ? timeLevel : scoreLevel;

            int new_timer = BASE_TIMER_MS - (level * 15);
            if (new_timer < MIN_TIMER_MS) new_timer = MIN_TIMER_MS;
            timer_ms = new_timer;
        }

        // RENDER
        winW = GetScreenWidth();
        winH = GetScreenHeight();
        // compute dynamic grid anchor
        GRID_X = winW * 0.10f;
        GRID_Y = winH * 0.10f;

        // dynamic layout anchor points
        // Dynamically scale cell size based on window height
        CELL = (winH - GRID_Y - 40) / ROWS;

        // Hard limits so it doesn't become microscopic or insane
        if (CELL < 16) CELL = 16;
        if (CELL > 48) CELL = 48;



        BeginDrawing();
        ClearBackground((Color){20, 20, 20, 255});
        DrawRectangleGradientV(0, 0, winW, winH,
                               (Color){10,10,10,255}, (Color){35,35,35,255});

        float sidebarX = winW * 0.70f;
        float sidebarY = winH * 0.10f;

        float t = GetTime();
        int r = (int)(128 + 127 * sin(t * 2.0));
        int g = (int)(128 + 127 * sin(t * 2.0 + 2.0));
        int b = (int)(128 + 127 * sin(t * 2.0 + 4.0));
        Color titleColor = (Color){r, g, b, 255};

        DrawText("TETRIS", GRID_X, 20, 32, titleColor);

        int elapsed = (int)(GetTime() - startTime);

        drawSidebar(player, elapsed);

        float controlBoxX = winW * 0.70f - 15;
        float controlBoxY = winH * 0.60f;

        float controlBoxW = CELL * 10;
        float controlBoxH = CELL * 6;



        DrawRectangle(controlBoxX, controlBoxY, controlBoxW, controlBoxH,
                      Fade((Color){50,50,50,255}, 0.6f));
        DrawRectangleLines(controlBoxX, controlBoxY, controlBoxW, controlBoxH,
                           (Color){90,90,90,255});
            float textX = controlBoxX + 20;   // padding inside the box
            float textY = controlBoxY + 10;

            DrawText("Controls:", textX, textY, 16, RAYWHITE);
            textY += 25;

            DrawText("A/D or Left/Right : Move", textX, textY, 14, LIGHTGRAY);
            textY += 20;

            DrawText("S or Down         : Soft Drop", textX, textY, 14, LIGHTGRAY);
            textY += 20;

            DrawText("Space             : Hard Drop", textX, textY, 14, LIGHTGRAY);
            textY += 20;

            DrawText("W or Up           : Rotate", textX, textY, 14, LIGHTGRAY);
            textY += 20;

            DrawText("P: Pause   Q: Quit", textX, textY, 14, LIGHTGRAY);



        drawGridAndCurrent();   // draw grid normally

        if (paused) {
            DrawRectangle(0, 0, winW, winH, Fade(BLACK, 0.6f));  // overlay ABOVE EVERYTHING
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

        // Simple top-out check: if immediately on spawn invalid, GameOn is FALSE
        if (!GameOn) break;
    }

    safe_free_shape(&current);
    freeQueue();

    if (!WindowShouldClose()) {
        int result = showGameOverScreen(player->name);
        int saveChoice = askSaveDataScreen(score);

        /// update stats only if user saved
    if (saveChoice == 1) {
        saveScore(player->name, score);
        player->timesPlayed++;

        if (score > player->bestScore)
            player->bestScore = score;

        double endTime = GetTime();
        int sessionTime = (int)(endTime - startTime);
        player->totalTime += sessionTime;

        saveProfiles();
    }


        // result handling
        if (result == 1) {   // Retry
            resetGameState();
            playGame(player);   // actually restart the game
            return;
        }


        if (result == 2) {  // E = back to menu
            return;         // return to mainMenu_GUI()
        }

        // result == 0 = quit the whole thing

            }

}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    srand((unsigned)time(NULL));

    loadProfiles();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(1000, 800, "Tetris");
    SetTargetFPS(144);

    currentPlayer = getPlayerFromGUI();
    if (!currentPlayer) {
        CloseWindow();
        return 0;
    }

    while (!WindowShouldClose()) {
        int choice = mainMenu_GUI(currentPlayer);

        switch (choice) {
            case 1:  // New Game
                playGame(currentPlayer);
                break;

            case 2:  // Player Manager
                playerManagerMenu_GUI();
                break;

            case 3:  // Quit
                CloseWindow();
                return 0;

            default:
                break;
        }

    }

    CloseWindow();
    return 0;
}
