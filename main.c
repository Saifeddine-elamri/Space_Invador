#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

// Window dimensions
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Game constants
#define PLAYER_WIDTH 60
#define PLAYER_HEIGHT 40
#define PLAYER_SPEED 8
#define PLAYER_BULLET_SPEED 12
#define ALIEN_ROWS 5
#define ALIEN_COLS 11
#define ALIEN_WIDTH 40
#define ALIEN_HEIGHT 40
#define ALIEN_SPACING_H 20
#define ALIEN_SPACING_V 15
#define ALIEN_BULLET_SPEED 6
#define ALIEN_MOVE_SPEED 2
#define MAX_PLAYER_BULLETS 3
#define MAX_ALIEN_BULLETS 8
#define SHIELD_COUNT 4
#define SHIELD_WIDTH 80
#define SHIELD_HEIGHT 60
#define SHIELD_BLOCK_SIZE 8
#define EXPLOSION_FRAMES 8
#define EXPLOSION_DURATION 4

// Game states
typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_OVER,
    GAME_WIN
} GameState;

// Direction
typedef enum {
    DIR_LEFT,
    DIR_RIGHT
} Direction;

// Entity types
typedef enum {
    ENTITY_PLAYER,
    ENTITY_ALIEN,
    ENTITY_PLAYER_BULLET,
    ENTITY_ALIEN_BULLET,
    ENTITY_SHIELD,
    ENTITY_EXPLOSION
} EntityType;

// Bullet structure
typedef struct {
    int x, y;
    bool active;
} Bullet;

// Alien structure
typedef struct {
    int x, y;
    int type; // 0, 1, or 2 for different alien types
    bool alive;
} Alien;

// Shield block structure
typedef struct {
    int x, y;
    bool active;
} ShieldBlock;

// Shield structure
typedef struct {
    int x, y;
    ShieldBlock blocks[SHIELD_WIDTH/SHIELD_BLOCK_SIZE][SHIELD_HEIGHT/SHIELD_BLOCK_SIZE];
} Shield;

// Explosion structure
typedef struct {
    int x, y;
    int frame;
    int timer;
    bool active;
} Explosion;

// Game structure
typedef struct {
    // Player
    int playerX, playerY;
    int playerLives;
    Bullet playerBullets[MAX_PLAYER_BULLETS];
    
    // Aliens
    Alien aliens[ALIEN_ROWS][ALIEN_COLS];
    int alienCount;
    Direction alienDirection;
    int alienMoveTimer;
    int alienMoveDelay;
    int alienDropDistance;
    Bullet alienBullets[MAX_ALIEN_BULLETS];
    int alienShootTimer;
    int alienShootDelay;
    
    // Shields
    Shield shields[SHIELD_COUNT];
    
    // Explosions
    Explosion explosions[20];
    
    // Game state
    GameState state;
    int score;
    int level;
    int gameOverTimer;
    
    // Window handle
    HWND hwnd;
} Game;

// Global game instance
Game game;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeGame(HWND hwnd);
void UpdateGame();
void RenderGame(HDC hdc);
void DrawPlayer(HDC hdc);
void DrawAliens(HDC hdc);
void DrawBullets(HDC hdc);
void DrawShields(HDC hdc);
void DrawExplosions(HDC hdc);
void DrawHUD(HDC hdc);
void DrawMenu(HDC hdc);
void DrawGameOver(HDC hdc);
void DrawWin(HDC hdc);
void MovePlayer(int direction);
void FirePlayerBullet();
void FireAlienBullet();
void MoveAliens();
void CheckCollisions();
void CreateExplosion(int x, int y);
void InitializeLevel();
void InitializeShields();

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register the window class
    const char CLASS_NAME[] = "SpaceInvadersClass";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    
    RegisterClass(&wc);
    
    // Create the window
    HWND hwnd = CreateWindowEx(
        0,                          // Optional window styles
        CLASS_NAME,                 // Window class
        "Space Invaders",           // Window title
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,  // Window style - fixed size
        
        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH + 16, WINDOW_HEIGHT + 39,
        
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    
    if (hwnd == NULL) {
        return 0;
    }
    
    // Initialize random seed
    srand((unsigned int)time(NULL));
    
    // Initialize the game
    game.hwnd = hwnd;
    InitializeGame(hwnd);
    
    // Show the window
    ShowWindow(hwnd, nCmdShow);
    
    // Set up timer for game updates (16ms interval ≈ 60 FPS)
    SetTimer(hwnd, 1, 16, NULL);
    
    // Run the message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RenderGame(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER:
            UpdateGame();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
            
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_LEFT:
                    if (game.state == GAME_PLAYING) {
                        MovePlayer(-1);
                    }
                    break;
                    
                case VK_RIGHT:
                    if (game.state == GAME_PLAYING) {
                        MovePlayer(1);
                    }
                    break;
                    
                case VK_SPACE:
                    if (game.state == GAME_PLAYING) {
                        FirePlayerBullet();
                    } else if (game.state == GAME_MENU) {
                        game.state = GAME_PLAYING;
                        InitializeLevel();
                    } else if (game.state == GAME_OVER || game.state == GAME_WIN) {
                        InitializeGame(hwnd);
                    }
                    break;
                    
                case VK_ESCAPE:
                    if (game.state == GAME_PLAYING) {
                        game.state = GAME_MENU;
                    } else if (game.state == GAME_MENU) {
                        DestroyWindow(hwnd);
                    }
                    break;
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Initialize the game
void InitializeGame(HWND hwnd) {
    // Initialize game state
    game.state = GAME_MENU;
    game.score = 0;
    game.level = 1;
    game.playerLives = 3;
    
    // Initialize player
    game.playerX = (WINDOW_WIDTH - PLAYER_WIDTH) / 2;
    game.playerY = WINDOW_HEIGHT - PLAYER_HEIGHT - 20;
    
    // Initialize player bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        game.playerBullets[i].active = false;
    }
    
    // Initialize alien bullets
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        game.alienBullets[i].active = false;
    }
    
    // Initialize explosions
    for (int i = 0; i < 20; i++) {
        game.explosions[i].active = false;
    }
    
    // Initialize aliens
    InitializeLevel();
    
    // Initialize shields
    InitializeShields();
}

// Initialize level
void InitializeLevel() {
    // Initialize aliens
    game.alienCount = 0;
    for (int row = 0; row < ALIEN_ROWS; row++) {
        for (int col = 0; col < ALIEN_COLS; col++) {
            game.aliens[row][col].x = 100 + col * (ALIEN_WIDTH + ALIEN_SPACING_H);
            game.aliens[row][col].y = 80 + row * (ALIEN_HEIGHT + ALIEN_SPACING_V);
            game.aliens[row][col].type = row < 1 ? 0 : (row < 3 ? 1 : 2);
            game.aliens[row][col].alive = true;
            game.alienCount++;
        }
    }
    
    // Initialize alien movement
    game.alienDirection = DIR_RIGHT;
    game.alienMoveTimer = 0;
    game.alienMoveDelay = 30 - (game.level * 2);
    if (game.alienMoveDelay < 10) game.alienMoveDelay = 10;
    game.alienDropDistance = 20;
    
    // Initialize alien shooting
    game.alienShootTimer = 0;
    game.alienShootDelay = 60 - (game.level * 5);
    if (game.alienShootDelay < 20) game.alienShootDelay = 20;
    
    // Initialize shields
    InitializeShields();
}

// Initialize shields
void InitializeShields() {
    int shieldSpacing = (WINDOW_WIDTH - (SHIELD_COUNT * SHIELD_WIDTH)) / (SHIELD_COUNT + 1);
    
    for (int s = 0; s < SHIELD_COUNT; s++) {
        game.shields[s].x = shieldSpacing + s * (SHIELD_WIDTH + shieldSpacing);
        game.shields[s].y = WINDOW_HEIGHT - 150;
        
        // Initialize shield blocks
        for (int x = 0; x < SHIELD_WIDTH/SHIELD_BLOCK_SIZE; x++) {
            for (int y = 0; y < SHIELD_HEIGHT/SHIELD_BLOCK_SIZE; y++) {
                // Create shield shape (arch)
                bool isActive = true;
                
                // Create an arch shape
                if (y > (SHIELD_HEIGHT/SHIELD_BLOCK_SIZE) * 0.6 && 
                    x > (SHIELD_WIDTH/SHIELD_BLOCK_SIZE) * 0.3 && 
                    x < (SHIELD_WIDTH/SHIELD_BLOCK_SIZE) * 0.7) {
                    isActive = false;
                }
                
                game.shields[s].blocks[x][y].x = game.shields[s].x + x * SHIELD_BLOCK_SIZE;
                game.shields[s].blocks[x][y].y = game.shields[s].y + y * SHIELD_BLOCK_SIZE;
                game.shields[s].blocks[x][y].active = isActive;
            }
        }
    }
}

// Update game state
void UpdateGame() {
    if (game.state == GAME_PLAYING) {
        // Move aliens
        game.alienMoveTimer++;
        if (game.alienMoveTimer >= game.alienMoveDelay) {
            game.alienMoveTimer = 0;
            MoveAliens();
        }
        
        // Alien shooting
        game.alienShootTimer++;
        if (game.alienShootTimer >= game.alienShootDelay) {
            game.alienShootTimer = 0;
            FireAlienBullet();
        }
        
        // Move player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
            if (game.playerBullets[i].active) {
                game.playerBullets[i].y -= PLAYER_BULLET_SPEED;
                
                // Check if bullet is out of bounds
                if (game.playerBullets[i].y < 0) {
                    game.playerBullets[i].active = false;
                }
            }
        }
        
        // Move alien bullets
        for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
            if (game.alienBullets[i].active) {
                game.alienBullets[i].y += ALIEN_BULLET_SPEED;
                
                // Check if bullet is out of bounds
                if (game.alienBullets[i].y > WINDOW_HEIGHT) {
                    game.alienBullets[i].active = false;
                }
            }
        }
        
        // Update explosions
        for (int i = 0; i < 20; i++) {
            if (game.explosions[i].active) {
                game.explosions[i].timer++;
                if (game.explosions[i].timer >= EXPLOSION_DURATION) {
                    game.explosions[i].timer = 0;
                    game.explosions[i].frame++;
                    if (game.explosions[i].frame >= EXPLOSION_FRAMES) {
                        game.explosions[i].active = false;
                    }
                }
            }
        }
        
        // Check collisions
        CheckCollisions();
        
        // Check win condition
        if (game.alienCount == 0) {
            game.level++;
            if (game.level > 10) {
                game.state = GAME_WIN;
            } else {
                InitializeLevel();
            }
        }
    } else if (game.state == GAME_OVER) {
        game.gameOverTimer++;
        if (game.gameOverTimer > 180) { // 3 seconds at 60 FPS
            game.state = GAME_MENU;
        }
    }
}

// Render the game
void RenderGame(HDC hdc) {
    // Create memory DC for double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    HBITMAP oldBitmap = SelectObject(memDC, memBitmap);
    
    // Fill background with black
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    FillRect(memDC, &rect, blackBrush);
    DeleteObject(blackBrush);
    
    // Draw stars
    HBRUSH starBrush = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(memDC, starBrush);
    
    srand(12345); // Fixed seed for consistent star pattern
    
    for (int i = 0; i < 200; i++) {
        int x = rand() % WINDOW_WIDTH;
        int y = rand() % WINDOW_HEIGHT;
        int size = rand() % 3 + 1;
        
        Ellipse(memDC, x, y, x + size, y + size);
    }
    
    // Draw some distant galaxies/nebulae
    for (int i = 0; i < 5; i++) {
        int x = rand() % WINDOW_WIDTH;
        int y = rand() % WINDOW_HEIGHT;
        int size = rand() % 50 + 20;
        
        // Create a subtle colored glow
        COLORREF galaxyColor;
        switch (rand() % 3) {
            case 0: galaxyColor = RGB(50, 50, 150); break; // Blue
            case 1: galaxyColor = RGB(150, 50, 150); break; // Purple
            case 2: galaxyColor = RGB(150, 50, 50); break; // Red
        }
        
        HBRUSH galaxyBrush = CreateSolidBrush(galaxyColor);
        SelectObject(memDC, galaxyBrush);
        
        // Draw with low alpha (simulated by making small dots)
        for (int j = 0; j < 30; j++) {
            int offsetX = (rand() % size) - size/2;
            int offsetY = (rand() % size) - size/2;
            int dotSize = rand() % 2 + 1;
            
            Ellipse(memDC, x + offsetX, y + offsetY, x + offsetX + dotSize, y + offsetY + dotSize);
        }
        
        DeleteObject(galaxyBrush);
    }
    
    // Draw game elements based on game state
    switch (game.state) {
        case GAME_MENU:
            DrawMenu(memDC);
            break;
            
        case GAME_PLAYING:
            DrawShields(memDC);
            DrawPlayer(memDC);
            DrawAliens(memDC);
            DrawBullets(memDC);
            DrawExplosions(memDC);
            DrawHUD(memDC);
            break;
            
        case GAME_OVER:
            DrawShields(memDC);
            DrawAliens(memDC);
            DrawBullets(memDC);
            DrawExplosions(memDC);
            DrawHUD(memDC);
            DrawGameOver(memDC);
            break;
            
        case GAME_WIN:
            DrawWin(memDC);
            break;
    }
    
    // Copy from memory DC to screen
    BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);
    
    // Clean up
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

// Draw player ship
void DrawPlayer(HDC hdc) {
    // Draw player ship
    HBRUSH greenBrush = CreateSolidBrush(RGB(0, 240, 0));
    HPEN greenPen = CreatePen(PS_SOLID, 1, RGB(0, 240, 0));
    
    SelectObject(hdc, greenBrush);
    SelectObject(hdc, greenPen);
    
    // Draw ship body
    POINT shipBody[] = {
        {game.playerX + PLAYER_WIDTH/2, game.playerY},
        {game.playerX + PLAYER_WIDTH, game.playerY + PLAYER_HEIGHT},
        {game.playerX, game.playerY + PLAYER_HEIGHT}
    };
    Polygon(hdc, shipBody, 3);
    
    // Draw cockpit
    HBRUSH lightGreenBrush = CreateSolidBrush(RGB(150, 255, 150));
    SelectObject(hdc, lightGreenBrush);
    
    POINT cockpit[] = {
        {game.playerX + PLAYER_WIDTH/2, game.playerY + 10},
        {game.playerX + PLAYER_WIDTH/2 + 10, game.playerY + PLAYER_HEIGHT - 10},
        {game.playerX + PLAYER_WIDTH/2 - 10, game.playerY + PLAYER_HEIGHT - 10}
    };
    Polygon(hdc, cockpit, 3);
    
    // Clean up
    DeleteObject(greenBrush);
    DeleteObject(greenPen);
    DeleteObject(lightGreenBrush);
}

// Draw aliens
void DrawAliens(HDC hdc) {
    for (int row = 0; row < ALIEN_ROWS; row++) {
        for (int col = 0; col < ALIEN_COLS; col++) {
            if (game.aliens[row][col].alive) {
                int x = game.aliens[row][col].x;
                int y = game.aliens[row][col].y;
                int type = game.aliens[row][col].type;
                
                // Colors based on alien type
                COLORREF alienColor;
                switch (type) {
                    case 0: alienColor = RGB(255, 50, 50); break;  // Red
                    case 1: alienColor = RGB(50, 150, 255); break; // Blue
                    case 2: alienColor = RGB(255, 255, 50); break; // Yellow
                    default: alienColor = RGB(255, 50, 255); break; // Purple
                }
                
                HBRUSH alienBrush = CreateSolidBrush(alienColor);
                HPEN alienPen = CreatePen(PS_SOLID, 1, alienColor);
                
                SelectObject(hdc, alienBrush);
                SelectObject(hdc, alienPen);
                
                // Draw alien based on type
                switch (type) {
                    case 0: // Type 1 - UFO shape
                        Ellipse(hdc, x + 5, y + 10, x + ALIEN_WIDTH - 5, y + 30);
                        Rectangle(hdc, x + 15, y + 5, x + ALIEN_WIDTH - 15, y + 10);
                        
                        // Draw eyes
                        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                        SelectObject(hdc, whiteBrush);
                        Ellipse(hdc, x + 12, y + 15, x + 22, y + 25);
                        Ellipse(hdc, x + ALIEN_WIDTH - 22, y + 15, x + ALIEN_WIDTH - 12, y + 25);
                        
                        // Draw pupils
                        HBRUSH blackEyeBrush = CreateSolidBrush(RGB(0, 0, 0));
                        SelectObject(hdc, blackEyeBrush);
                        Ellipse(hdc, x + 15, y + 18, x + 19, y + 22);
                        Ellipse(hdc, x + ALIEN_WIDTH - 19, y + 18, x + ALIEN_WIDTH - 15, y + 22);
                        
                        // Draw legs
                        SelectObject(hdc, alienPen);
                        MoveToEx(hdc, x + 10, y + 30, NULL);
                        LineTo(hdc, x + 5, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + 20, y + 30, NULL);
                        LineTo(hdc, x + 15, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + ALIEN_WIDTH - 20, y + 30, NULL);
                        LineTo(hdc, x + ALIEN_WIDTH - 15, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + ALIEN_WIDTH - 10, y + 30, NULL);
                        LineTo(hdc, x + ALIEN_WIDTH - 5, y + ALIEN_HEIGHT - 5);
                        
                        DeleteObject(whiteBrush);
                        DeleteObject(blackEyeBrush);
                        break;
                        
                    case 1: // Type 2 - Crab-like
                        // Body
                        Ellipse(hdc, x + 10, y + 5, x + ALIEN_WIDTH - 10, y + 25);
                        
                        // Eyes
                        SelectObject(hdc, whiteBrush);
                        Ellipse(hdc, x + 15, y + 10, x + 22, y + 17);
                        Ellipse(hdc, x + ALIEN_WIDTH - 22, y + 10, x + ALIEN_WIDTH - 15, y + 17);
                        
                        // Claws
                        SelectObject(hdc, alienBrush);
                        Ellipse(hdc, x + 2, y + 15, x + 12, y + 25);
                        Ellipse(hdc, x + ALIEN_WIDTH - 12, y + 15, x + ALIEN_WIDTH - 2, y + 25);
                        
                        // Legs
                        MoveToEx(hdc, x + 15, y + 25, NULL);
                        LineTo(hdc, x + 10, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + ALIEN_WIDTH/2 - 5, y + 25, NULL);
                        LineTo(hdc, x + ALIEN_WIDTH/2 - 10, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + ALIEN_WIDTH/2 + 5, y + 25, NULL);
                        LineTo(hdc, x + ALIEN_WIDTH/2 + 10, y + ALIEN_HEIGHT - 5);
                        
                        MoveToEx(hdc, x + ALIEN_WIDTH - 15, y + 25, NULL);
                        LineTo(hdc, x + ALIEN_WIDTH - 10, y + ALIEN_HEIGHT - 5);
                        
                        DeleteObject(whiteBrush);
                        break;
                        
                    case 2: // Type 3 - Octopus-like
                        // Head
                        Ellipse(hdc, x + 10, y + 5, x + ALIEN_WIDTH - 10, y + 25);
                        
                        // Eyes
                        SelectObject(hdc, whiteBrush);
                        Ellipse(hdc, x + 15, y + 10, x + 22, y + 17);
                        Ellipse(hdc, x + ALIEN_WIDTH - 22, y + 10, x + ALIEN_WIDTH - 15, y + 17);
                        
                        // Tentacles
                        SelectObject(hdc, alienPen);
                        
                        for (int i = 0; i < 8; i++) {
                            int startX = x + 10 + (i * (ALIEN_WIDTH - 20) / 7);
                            MoveToEx(hdc, startX, y + 25, NULL);
                            
                            // Wavy tentacles
                            for (int j = 0; j < 3; j++) {
                                int offsetX = (j % 2 == 0) ? 3 : -3;
                                LineTo(hdc, startX + offsetX, y + 25 + (j+1) * 5);
                            }
                        }
                        
                        DeleteObject(whiteBrush);
                        break;
                }
                
                // Clean up
                DeleteObject(alienBrush);
                DeleteObject(alienPen);
            }
        }
    }
}

// Draw bullets
void DrawBullets(HDC hdc) {
    // Draw player bullets
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    
    SelectObject(hdc, whiteBrush);
    SelectObject(hdc, whitePen);
    
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (game.playerBullets[i].active) {
            Rectangle(hdc, 
                game.playerBullets[i].x - 1, 
                game.playerBullets[i].y, 
                game.playerBullets[i].x + 2, 
                game.playerBullets[i].y + 12);
        }
    }
    
    // Draw alien bullets
    HBRUSH redBrush = CreateSolidBrush(RGB(255, 100, 100));
    HPEN redPen = CreatePen(PS_SOLID, 1, RGB(255, 100, 100));
    
    SelectObject(hdc, redBrush);
    SelectObject(hdc, redPen);
    
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (game.alienBullets[i].active) {
            // Zigzag bullet
            POINT zigzag[] = {
                {game.alienBullets[i].x - 2, game.alienBullets[i].y},
                {game.alienBullets[i].x + 1, game.alienBullets[i].y + 3},
                {game.alienBullets[i].x - 2, game.alienBullets[i].y + 6},
                {game.alienBullets[i].x + 1, game.alienBullets[i].y + 9},
                {game.alienBullets[i].x - 2, game.alienBullets[i].y + 12},
                {game.alienBullets[i].x + 2, game.alienBullets[i].y + 12},
                {game.alienBullets[i].x - 1, game.alienBullets[i].y + 9},
                {game.alienBullets[i].x + 2, game.alienBullets[i].y + 6},
                {game.alienBullets[i].x - 1, game.alienBullets[i].y + 3},
                {game.alienBullets[i].x + 2, game.alienBullets[i].y}
            };
            Polygon(hdc, zigzag, 10);
        }
    }
    
    // Clean up
    DeleteObject(whiteBrush);
    DeleteObject(whitePen);
    DeleteObject(redBrush);
    DeleteObject(redPen);
}

// Draw shields
void DrawShields(HDC hdc) {
    HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
    HBRUSH oldBrush = SelectObject(hdc, greenBrush);
    
    for (int s = 0; s < SHIELD_COUNT; s++) {
        for (int x = 0; x < SHIELD_WIDTH/SHIELD_BLOCK_SIZE; x++) {
            for (int y = 0; y < SHIELD_HEIGHT/SHIELD_BLOCK_SIZE; y++) {
                if (game.shields[s].blocks[x][y].active) {
                    Rectangle(
                        hdc,
                        game.shields[s].blocks[x][y].x,
                        game.shields[s].blocks[x][y].y,
                        game.shields[s].blocks[x][y].x + SHIELD_BLOCK_SIZE,
                        game.shields[s].blocks[x][y].y + SHIELD_BLOCK_SIZE
                    );
                }
            }
        }
    }
    
    SelectObject(hdc, oldBrush);
    DeleteObject(greenBrush);
}

// Draw explosions
void DrawExplosions(HDC hdc) {
    for (int i = 0; i < 20; i++) {
        if (game.explosions[i].active) {
            int frame = game.explosions[i].frame;
            int x = game.explosions[i].x;
            int y = game.explosions[i].y;
            
            // Colors for explosion
            COLORREF colors[] = {
                RGB(255, 255, 100),  // Yellow
                RGB(255, 150, 50),   // Orange
                RGB(255, 50, 50),    // Red
                RGB(200, 50, 50)     // Dark red
            };
            
            int colorIndex = frame % 4;
            int size = 20 - frame * 2;
            if (size < 5) size = 5;
            
            int particles = 8 + frame * 2;
            float angleStep = 2 * 3.14159f / particles;
            
            HBRUSH particleBrush = CreateSolidBrush(colors[colorIndex]);
            SelectObject(hdc, particleBrush);
            
            // Draw explosion particles
            for (int j = 0; j < particles; j++) {
                float angle = j * angleStep;
                int distance = 5 + frame * 2;
                int particleX = x + (int)(cos(angle) * distance);
                int particleY = y + (int)(sin(angle) * distance);
                
                Ellipse(hdc, 
                    particleX - size/2, 
                    particleY - size/2, 
                    particleX + size/2, 
                    particleY + size/2);
            }
            
            // Draw center
            if (frame < 4) {
                HBRUSH centerBrush = CreateSolidBrush(RGB(255, 255, 255));
                SelectObject(hdc, centerBrush);
                Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);
                DeleteObject(centerBrush);
            }
            
            DeleteObject(particleBrush);
        }
    }
}

// Draw HUD (score, lives, level)
void DrawHUD(HDC hdc) {
    char scoreText[50];
    char livesText[20];
    char levelText[20];
    
    sprintf(scoreText, "SCORE: %d", game.score);
    sprintf(livesText, "LIVES: %d", game.playerLives);
    sprintf(levelText, "LEVEL: %d", game.level);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    
    // Create font
    HFONT font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                           DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT oldFont = SelectObject(hdc, font);
    
    // Draw text
    TextOut(hdc, 20, 20, scoreText, strlen(scoreText));
    TextOut(hdc, WINDOW_WIDTH - 120, 20, livesText, strlen(livesText));
    TextOut(hdc, (WINDOW_WIDTH - 80) / 2, 20, levelText, strlen(levelText));
    
    // Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// Draw menu screen
void DrawMenu(HDC hdc) {
    const char* titleText = "SPACE INVADERS";
    const char* instructionText = "Press SPACE to Start";
    const char* controlsText1 = "Controls:";
    const char* controlsText2 = "LEFT/RIGHT - Move Ship";
    const char* controlsText3 = "SPACE - Fire";
    const char* controlsText4 = "ESC - Menu/Exit";
    
    // Set up text properties
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    
    // Create title font
    HFONT titleFont = CreateFont(60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                               DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT oldFont = SelectObject(hdc, titleFont);
    
    // Draw title
    RECT titleRect = {0, 100, WINDOW_WIDTH, 160};
    DrawText(hdc, titleText, -1, &titleRect, DT_CENTER | DT_SINGLELINE);
    
    // Create instruction font
    HFONT instructionFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, instructionFont);
    
    // Draw instruction
    RECT instructionRect = {0, 250, WINDOW_WIDTH, 290};
    DrawText(hdc, instructionText, -1, &instructionRect, DT_CENTER | DT_SINGLELINE);
    
    // Create controls font
    HFONT controlsFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, controlsFont);
    
    // Draw controls
    RECT controlsRect1 = {0, 350, WINDOW_WIDTH, 380};
    DrawText(hdc, controlsText1, -1, &controlsRect1, DT_CENTER | DT_SINGLELINE);
    
    RECT controlsRect2 = {0, 380, WINDOW_WIDTH, 410};
    DrawText(hdc, controlsText2, -1, &controlsRect2, DT_CENTER | DT_SINGLELINE);
    
    RECT controlsRect3 = {0, 410, WINDOW_WIDTH, 440};
    DrawText(hdc, controlsText3, -1, &controlsRect3, DT_CENTER | DT_SINGLELINE);
    
    RECT controlsRect4 = {0, 440, WINDOW_WIDTH, 470};
    DrawText(hdc, controlsText4, -1, &controlsRect4, DT_CENTER | DT_SINGLELINE);
    
    // Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(instructionFont);
    DeleteObject(controlsFont);
    
    // Draw some aliens for decoration
    for (int i = 0; i < 3; i++) {
        int x = 200 + i * 180;
        int y = 180;
        int type = i;
        
        // Colors based on alien type
        COLORREF alienColor;
        switch (type) {
            case 0: alienColor = RGB(255, 50, 50); break;  // Red
            case 1: alienColor = RGB(50, 150, 255); break; // Blue
            case 2: alienColor = RGB(255, 255, 50); break; // Yellow
        }
        
        HBRUSH alienBrush = CreateSolidBrush(alienColor);
        HPEN alienPen = CreatePen(PS_SOLID, 1, alienColor);
        
        SelectObject(hdc, alienBrush);
        SelectObject(hdc, alienPen);
        
        // Draw simple alien shape
        Ellipse(hdc, x + 5, y + 5, x + ALIEN_WIDTH - 5, y + ALIEN_HEIGHT - 5);
        
        // Draw eyes
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        SelectObject(hdc, whiteBrush);
        Ellipse(hdc, x + 12, y + 15, x + 18, y + 21);
        Ellipse(hdc, x + ALIEN_WIDTH - 18, y + 15, x + ALIEN_WIDTH - 12, y + 21);
        
        DeleteObject(alienBrush);
        DeleteObject(alienPen);
        DeleteObject(whiteBrush);
    }
}

// Draw game over screen
void DrawGameOver(HDC hdc) {
    const char* gameOverText = "GAME OVER";
    char scoreText[50];
    sprintf(scoreText, "Final Score: %d", game.score);
    const char* restartText = "Press SPACE to Restart";
    
    // Set up text properties
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 0, 0));
    
    // Create game over font
    HFONT gameOverFont = CreateFont(60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT oldFont = SelectObject(hdc, gameOverFont);
    
    // Draw game over text
    RECT gameOverRect = {0, 200, WINDOW_WIDTH, 260};
    DrawText(hdc, gameOverText, -1, &gameOverRect, DT_CENTER | DT_SINGLELINE);
    
    // Create score font
    HFONT scoreFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                               DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, scoreFont);
    
    // Draw score text
    RECT scoreRect = {0, 280, WINDOW_WIDTH, 320};
    DrawText(hdc, scoreText, -1, &scoreRect, DT_CENTER | DT_SINGLELINE);
    
    // Create restart font
    HFONT restartFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, restartFont);
    
    // Draw restart text
    RECT restartRect = {0, 350, WINDOW_WIDTH, 380};
    DrawText(hdc, restartText, -1, &restartRect, DT_CENTER | DT_SINGLELINE);
    
    // Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(gameOverFont);
    DeleteObject(scoreFont);
    DeleteObject(restartFont);
}

// Draw win screen
void DrawWin(HDC hdc) {
    const char* winText = "YOU WIN!";
    char scoreText[50];
    sprintf(scoreText, "Final Score: %d", game.score);
    const char* restartText = "Press SPACE to Play Again";
    
    // Set up text properties
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 0));
    
    // Create win font
    HFONT winFont = CreateFont(60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                             DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT oldFont = SelectObject(hdc, winFont);
    
    // Draw win text
    RECT winRect = {0, 200, WINDOW_WIDTH, 260};
    DrawText(hdc, winText, -1, &winRect, DT_CENTER | DT_SINGLELINE);
    
    // Create score font
    HFONT scoreFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                               DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, scoreFont);
    
    // Draw score text
    RECT scoreRect = {0, 280, WINDOW_WIDTH, 320};
    DrawText(hdc, scoreText, -1, &scoreRect, DT_CENTER | DT_SINGLELINE);
    
    // Create restart font
    HFONT restartFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    SelectObject(hdc, restartFont);
    
    // Draw restart text
    RECT restartRect = {0, 350, WINDOW_WIDTH, 380};
    DrawText(hdc, restartText, -1, &restartRect, DT_CENTER | DT_SINGLELINE);
    
    // Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(winFont);
    DeleteObject(scoreFont);
    DeleteObject(restartFont);
}

// Move player
void MovePlayer(int direction) {
    game.playerX += direction * PLAYER_SPEED;
    
    // Keep player within bounds
    if (game.playerX < 0) {
        game.playerX = 0;
    } else if (game.playerX > WINDOW_WIDTH - PLAYER_WIDTH) {
        game.playerX = WINDOW_WIDTH - PLAYER_WIDTH;
    }
}

// Fire player bullet
void FirePlayerBullet() {
    // Find an inactive bullet
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!game.playerBullets[i].active) {
            game.playerBullets[i].active = true;
            game.playerBullets[i].x = game.playerX + PLAYER_WIDTH / 2;
            game.playerBullets[i].y = game.playerY;
            return;
        }
    }
}

// Fire alien bullet
void FireAlienBullet() {
    // Find an inactive bullet
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (!game.alienBullets[i].active) {
            // Find a random alien to shoot
            int attempts = 0;
            while (attempts < 50) {
                int row = rand() % ALIEN_ROWS;
                int col = rand() % ALIEN_COLS;
                
                if (game.aliens[row][col].alive) {
                    // Find the lowest alien in this column
                    int lowestRow = row;
                    for (int r = row + 1; r < ALIEN_ROWS; r++) {
                        if (game.aliens[r][col].alive) {
                            lowestRow = r;
                        }
                    }
                    
                    game.alienBullets[i].active = true;
                    game.alienBullets[i].x = game.aliens[lowestRow][col].x + ALIEN_WIDTH / 2;
                    game.alienBullets[i].y = game.aliens[lowestRow][col].y + ALIEN_HEIGHT;
                    return;
                }
                
                attempts++;
            }
            
            return;
        }
    }
}

// Move aliens
void MoveAliens() {
    bool shouldDropAndReverse = false;
    
    // Check if aliens should change direction
    if (game.alienDirection == DIR_RIGHT) {
        // Find rightmost alien
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = ALIEN_COLS - 1; col >= 0; col--) {
                if (game.aliens[row][col].alive) {
                    if (game.aliens[row][col].x + ALIEN_WIDTH + ALIEN_MOVE_SPEED > WINDOW_WIDTH) {
                        shouldDropAndReverse = true;
                        break;
                    }
                }
            }
            if (shouldDropAndReverse) break;
        }
    } else {
        // Find leftmost alien
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (game.aliens[row][col].alive) {
                    if (game.aliens[row][col].x - ALIEN_MOVE_SPEED < 0) {
                        shouldDropAndReverse = true;
                        break;
                    }
                }
            }
            if (shouldDropAndReverse) break;
        }
    }
    
    // Move aliens
    if (shouldDropAndReverse) {
        // Drop aliens
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (game.aliens[row][col].alive) {
                    game.aliens[row][col].y += game.alienDropDistance;
                    
                    // Check if aliens reached the bottom (player loses)
                    if (game.aliens[row][col].y + ALIEN_HEIGHT > game.playerY) {
                        game.playerLives = 0;
                        game.state = GAME_OVER;
                        game.gameOverTimer = 0;
                        return;
                    }
                }
            }
        }
        
        // Reverse direction
        game.alienDirection = (game.alienDirection == DIR_RIGHT) ? DIR_LEFT : DIR_RIGHT;
    } else {
        // Move aliens horizontally
        int moveAmount = (game.alienDirection == DIR_RIGHT) ? ALIEN_MOVE_SPEED : -ALIEN_MOVE_SPEED;
        
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (game.aliens[row][col].alive) {
                    game.aliens[row][col].x += moveAmount;
                }
            }
        }
    }
}

// Check collisions
void CheckCollisions() {
    // Player bullets vs aliens
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (game.playerBullets[i].active) {
            for (int row = 0; row < ALIEN_ROWS; row++) {
                for (int col = 0; col < ALIEN_COLS; col++) {
                    if (game.aliens[row][col].alive) {
                        if (game.playerBullets[i].x >= game.aliens[row][col].x &&
                            game.playerBullets[i].x <= game.aliens[row][col].x + ALIEN_WIDTH &&
                            game.playerBullets[i].y >= game.aliens[row][col].y &&
                            game.playerBullets[i].y <= game.aliens[row][col].y + ALIEN_HEIGHT) {
                            
                            // Hit alien
                            game.aliens[row][col].alive = false;
                            game.playerBullets[i].active = false;
                            game.alienCount--;
                            
                            // Add score based on alien type
                            switch (game.aliens[row][col].type) {
                                case 0: game.score += 30; break;
                                case 1: game.score += 20; break;
                                case 2: game.score += 10; break;
                            }
                            
                            // Create explosion
                            CreateExplosion(game.aliens[row][col].x + ALIEN_WIDTH / 2, 
                                           game.aliens[row][col].y + ALIEN_HEIGHT / 2);
                            
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Alien bullets vs player
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (game.alienBullets[i].active) {
            if (game.alienBullets[i].x >= game.playerX &&
                game.alienBullets[i].x <= game.playerX + PLAYER_WIDTH &&
                game.alienBullets[i].y >= game.playerY &&
                game.alienBullets[i].y <= game.playerY + PLAYER_HEIGHT) {
                
                // Hit player
                game.alienBullets[i].active = false;
                game.playerLives--;
                
                // Create explosion
                CreateExplosion(game.playerX + PLAYER_WIDTH / 2, game.playerY + PLAYER_HEIGHT / 2);
                
                // Check game over
                if (game.playerLives <= 0) {
                    game.state = GAME_OVER;
                    game.gameOverTimer = 0;
                }
                
                break;
            }
        }
    }
    
    // Bullets vs shields
    // Player bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (game.playerBullets[i].active) {
            for (int s = 0; s < SHIELD_COUNT; s++) {
                for (int x = 0; x < SHIELD_WIDTH/SHIELD_BLOCK_SIZE; x++) {
                    for (int y = 0; y < SHIELD_HEIGHT/SHIELD_BLOCK_SIZE; y++) {
                        if (game.shields[s].blocks[x][y].active) {
                            int blockX = game.shields[s].blocks[x][y].x;
                            int blockY = game.shields[s].blocks[x][y].y;
                            
                            if (game.playerBullets[i].x >= blockX &&
                                game.playerBullets[i].x <= blockX + SHIELD_BLOCK_SIZE &&
                                game.playerBullets[i].y >= blockY &&
                                game.playerBullets[i].y <= blockY + SHIELD_BLOCK_SIZE) {
                                
                                // Hit shield
                                game.shields[s].blocks[x][y].active = false;
                                game.playerBullets[i].active = false;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Alien bullets
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (game.alienBullets[i].active) {
            for (int s = 0; s < SHIELD_COUNT; s++) {
                for (int x = 0; x < SHIELD_WIDTH/SHIELD_BLOCK_SIZE; x++) {
                    for (int y = 0; y < SHIELD_HEIGHT/SHIELD_BLOCK_SIZE; y++) {
                        if (game.shields[s].blocks[x][y].active) {
                            int blockX = game.shields[s].blocks[x][y].x;
                            int blockY = game.shields[s].blocks[x][y].y;
                            
                            if (game.alienBullets[i].x >= blockX &&
                                game.alienBullets[i].x <= blockX + SHIELD_BLOCK_SIZE &&
                                game.alienBullets[i].y >= blockY &&
                                game.alienBullets[i].y <= blockY + SHIELD_BLOCK_SIZE) {
                                
                                // Hit shield
                                game.shields[s].blocks[x][y].active = false;
                                game.alienBullets[i].active = false;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Create explosion
void CreateExplosion(int x, int y) {
    for (int i = 0; i < 20; i++) {
        if (!game.explosions[i].active) {
            game.explosions[i].x = x;
            game.explosions[i].y = y;
            game.explosions[i].frame = 0;
            game.explosions[i].timer = 0;
            game.explosions[i].active = true;
            break;
        }
    }
}
