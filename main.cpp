#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h> 
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>
#include <fstream> 


const int SCREEN_WIDTH = 900;
const int SCREEN_HEIGHT = 650;
const int BOARD_SIZE = 4;
const int TILE_SIZE = 100;
const int TILE_MARGIN = 15;
const int BOARD_MARGIN = 10;
const int HEADER_HEIGHT = 150;
const char* FONT_PATH = "assets/fonts/arial.ttf";
const char* SAVE_FILE_SINGLE_PATH = "2048_save_single.dat"; 
const char* SAVE_FILE_MULTI_PATH = "2048_save_multi.dat"; 

const float ANIMATION_DURATION = 0.02f;  
const float NEW_TILE_DELAY = 0.04f;
const float NEW_TILE_ANIMATION_DURATION = 0.1f;
const float MERGE_ANIMATION_DURATION = 0.05f;

const char* SOUND_BUTTON = "assets/sounds/button.wav";
const char* SOUND_MOVE = "assets/sounds/move.wav";
const char* SOUND_MERGE = "assets/sounds/merge.wav";
const char* SOUND_MERGE_NEW = "assets/sounds/merge_new.wav";
const char* SOUND_GAMEOVER = "assets/sounds/gameover.wav";


enum GameState {
MENU,
PLAYING,
MULTIPLAYER,
GAME_OVER,
MULTIPLAYER_GAME_OVER,
HOW_TO_PLAY
};


enum PlayerTurn {
PLAYER_ONE,
PLAYER_TWO
};


enum AnimationState {
NONE,
MOVING,
APPEARING,
MERGING
};


struct TileAnimation {
int startRow, startCol;
int endRow, endCol;
float startTime;
float progress;
AnimationState state;
bool merged;
int value;
};


struct Button {
SDL_Rect rect;
std::string text;
bool isHovered;
};


struct Color {
Uint8 r, g, b;
};


const Color BACKGROUND_COLOR = {250, 248, 239}; // Light cream background
const Color BOARD_COLOR = {187, 173, 160}; // Tan board background
const Color EMPTY_TILE_COLOR = {205, 193, 180}; // Empty tile color
const Color TEXT_COLOR = {119, 110, 101}; // Dark brown text
const Color LIGHT_TEXT = {249, 246, 242}; // White text for dark tiles
const Color BUTTON_COLOR = {143, 122, 102};
const Color BUTTON_HOVER_COLOR = {156, 135, 115};
const Color MENU_BACKGROUND_COLOR = {250, 248, 239};
const Color PLAYER_ONE_COLOR = {231, 180, 180}; // Light red for player 1
const Color PLAYER_TWO_COLOR = {180, 200, 231}; // Light blue for player 2

// Helper function to create a Color
Color makeColor(Uint8 r, Uint8 g, Uint8 b) {
Color c;
c.r = r;
c.g = g;
c.b = b;
return c;
}

// Initialize TILE_COLORS vector with colors matching the original game
std::vector<Color> initTileColors() {
std::vector<Color> colors;
colors.push_back(makeColor(238, 228, 218)); // 2
colors.push_back(makeColor(237, 224, 200)); // 4
colors.push_back(makeColor(242, 177, 121)); // 8
colors.push_back(makeColor(245, 149, 99));  // 16
colors.push_back(makeColor(246, 124, 95));  // 32
colors.push_back(makeColor(246, 94, 59));   // 64
colors.push_back(makeColor(237, 207, 114)); // 128
colors.push_back(makeColor(237, 204, 97));  // 256
colors.push_back(makeColor(237, 200, 80));  // 512
colors.push_back(makeColor(237, 197, 63));  // 1024
colors.push_back(makeColor(237, 194, 46));  // 2048
return colors;
}

const std::vector<Color> TILE_COLORS = initTileColors();

// Helper function to convert our Color to SDL_Color
SDL_Color toSDLColor(const Color& color, Uint8 a = 255) {
SDL_Color sdlColor;
sdlColor.r = color.r;
sdlColor.g = color.g;
sdlColor.b = color.b;
sdlColor.a = a;
return sdlColor;
}

// Helper function to check if a point is inside a rectangle
bool isPointInRect(int x, int y, const SDL_Rect& rect) {
return (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h);
}

// Helper function to draw a rounded rectangle
void drawRoundedRect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius) {
// Draw the main rectangle (excluding corners)
SDL_Rect rect;
rect.x = x + radius;
rect.y = y;
rect.w = w - 2 * radius;
rect.h = h;
SDL_RenderFillRect(renderer, &rect);

rect.x = x;
rect.y = y + radius;
rect.w = w;
rect.h = h - 2 * radius;
SDL_RenderFillRect(renderer, &rect);

// Draw the four corner circles
int diameter = radius * 2;
for (int i = 0; i <= diameter; i++) {
    for (int j = 0; j <= diameter; j++) {
        int dx = radius - i;
        int dy = radius - j;
        if (dx*dx + dy*dy <= radius*radius) {
            // Top-left corner
            SDL_RenderDrawPoint(renderer, x + i, y + j);
            // Top-right corner
            SDL_RenderDrawPoint(renderer, x + w - i - 1, y + j);
            // Bottom-left corner
            SDL_RenderDrawPoint(renderer, x + i, y + h - j - 1);
            // Bottom-right corner
            SDL_RenderDrawPoint(renderer, x + w - i - 1, y + h - j - 1);
        }
    }
}
}

// Helper function for linear interpolation
float lerp(float a, float b, float t) {
return a + t * (b - a);
}

// Helper function for easing in and out - matches traditional 2048 animation curve
float easeInOut(float t) {
// Quadratic easing for smoother motion like the original 2048
return t < 0.5f ? 2.0f * t * t : 1.0f - pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

// Game class
class Game2048 {
private:
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;
TTF_Font* titleFont;
TTF_Font* menuFont;
TTF_Font* largeFont;
std::vector<std::vector<int> > board;
std::vector<std::vector<int> > boardP2; // Board for player 2
std::vector<std::vector<int> > previousBoard; // For animation
std::vector<std::vector<int> > previousBoardP2; // For animation
int score;
int scoreP2; // Score for player 2
int bestScore;
bool gameOver;
bool gameOverP2; // Game over state for player 2
bool won;
bool wonP2; // Win state for player 2
std::mt19937 rng;
GameState currentState;
PlayerTurn currentPlayer; // Current player in multiplayer mode
std::vector<Button> menuButtons;
std::vector<Button> howToPlayButtons;
std::vector<Button> gameOverButtons;
std::vector<Button> multiplayerGameOverButtons;
int mouseX, mouseY;

// Animation related variables
std::chrono::steady_clock::time_point lastFrameTime;
float deltaTime;
bool animating;
std::vector<TileAnimation> animations;
std::vector<TileAnimation> animationsP2;
std::map<std::pair<int, int>, std::pair<int, int>> mergedTiles; // Maps destination to source
std::map<std::pair<int, int>, std::pair<int, int>> mergedTilesP2;
std::vector<std::pair<int, int>> newTiles;
std::vector<std::pair<int, int>> newTilesP2;

// Texture caching for smoother animations
SDL_Texture* boardTexture;
bool boardTextureNeedsUpdate;

// Sound effects
Mix_Chunk* buttonSound;
Mix_Chunk* moveSound;
Mix_Chunk* mergeSound;
Mix_Chunk* mergeNewSound;
Mix_Chunk* gameoverSound;

// Biến để theo dõi thời gian lưu game tự động
Uint32 lastAutoSaveTime;
const Uint32 AUTO_SAVE_INTERVAL = 5000; // Lưu game mỗi 5 giây

public:
Game2048() : window(nullptr), renderer(nullptr), font(nullptr), titleFont(nullptr), 
             menuFont(nullptr), largeFont(nullptr), score(0), scoreP2(0), bestScore(0), 
             gameOver(false), gameOverP2(false), won(false), wonP2(false), 
             currentState(MENU), currentPlayer(PLAYER_ONE), mouseX(0), mouseY(0),
             deltaTime(0.0f), animating(false), boardTexture(nullptr), boardTextureNeedsUpdate(true),
             buttonSound(nullptr), moveSound(nullptr), mergeSound(nullptr), 
             mergeNewSound(nullptr), gameoverSound(nullptr), lastAutoSaveTime(0) {
    // Initialize random number generator
    std::random_device rd;
    rng = std::mt19937(rd());

    // Initialize boards
    board.resize(BOARD_SIZE);
    boardP2.resize(BOARD_SIZE);
    previousBoard.resize(BOARD_SIZE);
    previousBoardP2.resize(BOARD_SIZE);
    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i].resize(BOARD_SIZE, 0);
        boardP2[i].resize(BOARD_SIZE, 0);
        previousBoard[i].resize(BOARD_SIZE, 0);
        previousBoardP2[i].resize(BOARD_SIZE, 0);
    }
    
    // Initialize time
    lastFrameTime = std::chrono::steady_clock::now();
}

~Game2048() {
    // Lưu game trước khi thoát
    saveGame();
    
    // Free sound effects
    if (buttonSound != nullptr) Mix_FreeChunk(buttonSound);
    if (moveSound != nullptr) Mix_FreeChunk(moveSound);
    if (mergeSound != nullptr) Mix_FreeChunk(mergeSound);
    if (mergeNewSound != nullptr) Mix_FreeChunk(mergeNewSound);
    if (gameoverSound != nullptr) Mix_FreeChunk(gameoverSound);
    
    // Close SDL_mixer
    Mix_CloseAudio();
    
    // Giải phóng các texture tĩnh từ renderGameUI
    static SDL_Texture* bestLabelTexture = nullptr;
    static SDL_Texture* bestScoreTexture = nullptr;
    static SDL_Texture* scoreLabelTexture = nullptr;
    static SDL_Texture* scoreTexture = nullptr;
    
    if (bestLabelTexture != nullptr) SDL_DestroyTexture(bestLabelTexture);
    if (bestScoreTexture != nullptr) SDL_DestroyTexture(bestScoreTexture);
    if (scoreLabelTexture != nullptr) SDL_DestroyTexture(scoreLabelTexture);
    if (scoreTexture != nullptr) SDL_DestroyTexture(scoreTexture);
    
    if (boardTexture != nullptr) SDL_DestroyTexture(boardTexture);
    if (font != nullptr) TTF_CloseFont(font);
    if (titleFont != nullptr) TTF_CloseFont(titleFont);
    if (menuFont != nullptr) TTF_CloseFont(menuFont);
    if (largeFont != nullptr) TTF_CloseFont(largeFont);
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
}

// Hàm lưu trạng thái game vào file
void saveGame() {
    const char* filePath = (currentState == MULTIPLAYER || currentState == MULTIPLAYER_GAME_OVER) 
                          ? SAVE_FILE_MULTI_PATH : SAVE_FILE_SINGLE_PATH;
    
    std::ofstream saveFile(filePath, std::ios::binary);
    if (!saveFile.is_open()) {
        std::cerr << "Không thể mở file để lưu game!" << std::endl;
        return;
    }
    
    // Lưu trạng thái game hiện tại
    int stateInt = static_cast<int>(currentState);
    saveFile.write(reinterpret_cast<char*>(&stateInt), sizeof(int));
    
    // Lưu điểm số
    saveFile.write(reinterpret_cast<char*>(&score), sizeof(int));
    saveFile.write(reinterpret_cast<char*>(&scoreP2), sizeof(int));
    saveFile.write(reinterpret_cast<char*>(&bestScore), sizeof(int));
    
    // Lưu trạng thái game over và win
    saveFile.write(reinterpret_cast<char*>(&gameOver), sizeof(bool));
    saveFile.write(reinterpret_cast<char*>(&gameOverP2), sizeof(bool));
    saveFile.write(reinterpret_cast<char*>(&won), sizeof(bool));
    saveFile.write(reinterpret_cast<char*>(&wonP2), sizeof(bool));
    
    // Lưu người chơi hiện tại
    int playerInt = static_cast<int>(currentPlayer);
    saveFile.write(reinterpret_cast<char*>(&playerInt), sizeof(int));
    
    // Lưu bảng của người chơi 1
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            saveFile.write(reinterpret_cast<char*>(&board[i][j]), sizeof(int));
        }
    }
    
    // Lưu bảng của người chơi 2
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            saveFile.write(reinterpret_cast<char*>(&boardP2[i][j]), sizeof(int));
        }
    }
    
    saveFile.close();
    std::cout << "Đã lưu game thành công!" << std::endl;
}

// Hàm tải trạng thái game từ file
bool loadGame(bool isMultiplayer = false) {
    const char* filePath = isMultiplayer ? SAVE_FILE_MULTI_PATH : SAVE_FILE_SINGLE_PATH;
    
    std::ifstream saveFile(filePath, std::ios::binary);
    if (!saveFile.is_open()) {
        std::cout << "Không tìm thấy file lưu game, bắt đầu game mới." << std::endl;
        return false;
    }
    
    // Đọc trạng thái game
    int stateInt;
    saveFile.read(reinterpret_cast<char*>(&stateInt), sizeof(int));
    currentState = static_cast<GameState>(stateInt);
    
    // Đọc điểm số
    saveFile.read(reinterpret_cast<char*>(&score), sizeof(int));
    saveFile.read(reinterpret_cast<char*>(&scoreP2), sizeof(int));
    saveFile.read(reinterpret_cast<char*>(&bestScore), sizeof(int));
    
    // Đọc trạng thái game over và win
    saveFile.read(reinterpret_cast<char*>(&gameOver), sizeof(bool));
    saveFile.read(reinterpret_cast<char*>(&gameOverP2), sizeof(bool));
    saveFile.read(reinterpret_cast<char*>(&won), sizeof(bool));
    saveFile.read(reinterpret_cast<char*>(&wonP2), sizeof(bool));
    
    // Đọc người chơi hiện tại
    int playerInt;
    saveFile.read(reinterpret_cast<char*>(&playerInt), sizeof(int));
    currentPlayer = static_cast<PlayerTurn>(playerInt);
    
    // Đọc bảng của người chơi 1
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            saveFile.read(reinterpret_cast<char*>(&board[i][j]), sizeof(int));
        }
    }
    
    // Đọc bảng của người chơi 2
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            saveFile.read(reinterpret_cast<char*>(&boardP2[i][j]), sizeof(int));
        }
    }
    
    saveFile.close();
    
    // Đánh dấu cần cập nhật texture bảng
    boardTextureNeedsUpdate = true;
    
    std::cout << "Đã tải game thành công!" << std::endl;
    return true;
}

bool initialize() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        return false;
    }
    
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << std::endl;
        // Continue without sound
    } else {
        // Load sound effects
        buttonSound = Mix_LoadWAV(SOUND_BUTTON);
        moveSound = Mix_LoadWAV(SOUND_MOVE);
        mergeSound = Mix_LoadWAV(SOUND_MERGE);
        mergeNewSound = Mix_LoadWAV(SOUND_MERGE_NEW);
        gameoverSound = Mix_LoadWAV(SOUND_GAMEOVER);
        
        if (!buttonSound || !moveSound || !mergeSound || !mergeNewSound || !gameoverSound) {
            std::cerr << "Warning: Could not load sound effects! Mix_Error: " << Mix_GetError() << std::endl;
            // Continue without sound
        }
    }

    // Create window
    window = SDL_CreateWindow("2048 Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                             SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer with vsync enabled to prevent tearing
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Load fonts with smaller sizes
    font = TTF_OpenFont(FONT_PATH, 24); // Smaller font for tile numbers and buttons
    titleFont = TTF_OpenFont(FONT_PATH, 24); // Smaller title font
    menuFont = TTF_OpenFont(FONT_PATH, 28); // Smaller menu font
    largeFont = TTF_OpenFont(FONT_PATH, 48); // Smaller large font for title
    
    if (font == nullptr || titleFont == nullptr || menuFont == nullptr || largeFont == nullptr) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
        std::cerr << "Make sure the font file is located at " << FONT_PATH << std::endl;
    
        // Thử tìm font ở vị trí khác
        const char* altFontPath = "arial.ttf";
        font = TTF_OpenFont(altFontPath, 22);
        titleFont = TTF_OpenFont(altFontPath, 36);
        menuFont = TTF_OpenFont(altFontPath, 24);
        largeFont = TTF_OpenFont(altFontPath, 54);
    
        if (font == nullptr || titleFont == nullptr || menuFont == nullptr || largeFont == nullptr) {
            std::cerr << "Also tried " << altFontPath << " but failed." << std::endl;
            return false;
        } else {
            std::cout << "Successfully loaded font from " << altFontPath << std::endl;
        }
    }

    // Initialize menu buttons
    initializeMenuButtons();
    
    // Thử tải game đã lưu
    if (!loadGame() && !loadGame(true)) {
        // Nếu không có file lưu nào, bắt đầu với menu
        currentState = MENU;
    }

    return true;
}

// Play a sound effect
void playSound(Mix_Chunk* sound) {
    if (sound) {
        Mix_PlayChannel(-1, sound, 0);
    }
}

void initializeMenuButtons() {
    // Main menu buttons
    menuButtons.clear();
    
    Button singleplayerBtn;
    singleplayerBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    singleplayerBtn.rect.y = 200;
    singleplayerBtn.rect.w = 300;
    singleplayerBtn.rect.h = 60;
    singleplayerBtn.text = "Singleplayer";
    singleplayerBtn.isHovered = false;
    menuButtons.push_back(singleplayerBtn);
    
    Button multiplayerBtn;
    multiplayerBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    multiplayerBtn.rect.y = 280;
    multiplayerBtn.rect.w = 300;
    multiplayerBtn.rect.h = 60;
    multiplayerBtn.text = "Multiplayer";
    multiplayerBtn.isHovered = false;
    menuButtons.push_back(multiplayerBtn);
    
    Button howToPlayBtn;
    howToPlayBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    howToPlayBtn.rect.y = 360;
    howToPlayBtn.rect.w = 300;
    howToPlayBtn.rect.h = 60;
    howToPlayBtn.text = "How to Play";
    howToPlayBtn.isHovered = false;
    menuButtons.push_back(howToPlayBtn);
    
    Button exitBtn;
    exitBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    exitBtn.rect.y = 440;
    exitBtn.rect.w = 300;
    exitBtn.rect.h = 60;
    exitBtn.text = "Exit";
    exitBtn.isHovered = false;
    menuButtons.push_back(exitBtn);
    
    // How to play screen buttons
    howToPlayButtons.clear();
    
    Button backBtn;
    backBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    backBtn.rect.y = 575;
    backBtn.rect.w = 300;
    backBtn.rect.h = 60;
    backBtn.text = "Back to Menu";
    backBtn.isHovered = false;
    howToPlayButtons.push_back(backBtn);
    
    // Game over buttons
    gameOverButtons.clear();
    
    Button restartBtn;
    restartBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    restartBtn.rect.y = 350;
    restartBtn.rect.w = 300;
    restartBtn.rect.h = 60;
    restartBtn.text = "Play Again";
    restartBtn.isHovered = false;
    gameOverButtons.push_back(restartBtn);
    
    Button menuBtn;
    menuBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    menuBtn.rect.y = 430;
    menuBtn.rect.w = 300;
    menuBtn.rect.h = 60;
    menuBtn.text = "Main Menu";
    menuBtn.isHovered = false;
    gameOverButtons.push_back(menuBtn);
    
    // Multiplayer game over buttons
    multiplayerGameOverButtons.clear();
    
    Button mpRestartBtn;
    mpRestartBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    mpRestartBtn.rect.y = 400;
    mpRestartBtn.rect.w = 300;
    mpRestartBtn.rect.h = 60;
    mpRestartBtn.text = "Play Again";
    mpRestartBtn.isHovered = false;
    multiplayerGameOverButtons.push_back(mpRestartBtn);
    
    Button mpMenuBtn;
    mpMenuBtn.rect.x = SCREEN_WIDTH / 2 - 150;
    mpMenuBtn.rect.y = 480;
    mpMenuBtn.rect.w = 300;
    mpMenuBtn.rect.h = 60;
    mpMenuBtn.text = "Main Menu";
    mpMenuBtn.isHovered = false;
    multiplayerGameOverButtons.push_back(mpMenuBtn);
}

void addRandomTile() {
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    std::vector<std::pair<int, int> >& currentNewTiles = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? newTilesP2 : newTiles;
    
    std::vector<std::pair<int, int> > emptyCells;
    
    // Find all empty cells
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (currentBoard[i][j] == 0) {
                emptyCells.push_back(std::make_pair(i, j));
            }
        }
    }
    
    if (emptyCells.empty()) return;
    
    // Choose a random empty cell
    std::uniform_int_distribution<int> dist(0, emptyCells.size() - 1);
    int index = dist(rng);
    int row = emptyCells[index].first;
    int col = emptyCells[index].second;
    
    // 90% chance for a 2, 10% chance for a 4
    std::uniform_int_distribution<int> valueDist(0, 9);
    currentBoard[row][col] = (valueDist(rng) < 9) ? 2 : 4;
    
    // Add to new tiles for animation
    currentNewTiles.push_back(std::make_pair(row, col));
    
    // Mark the board texture as needing update
    boardTextureNeedsUpdate = true;
    
    // Play new tile sound
    playSound(mergeNewSound);
}

bool canMove(const std::vector<std::vector<int> >& checkBoard) {
    // Check for empty cells
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (checkBoard[i][j] == 0) return true;
        }
    }
    
    // Check for adjacent cells with the same value
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (j < BOARD_SIZE - 1 && checkBoard[i][j] == checkBoard[i][j + 1] && checkBoard[i][j] != 0) return true;
            if (i < BOARD_SIZE - 1 && checkBoard[i][j] == checkBoard[i][j + 1] && checkBoard[i][j] != 0) return true;
            if (i < BOARD_SIZE - 1 && checkBoard[i][j] == checkBoard[i + 1][j] && checkBoard[i][j] != 0) return true;
        }
    }
    
    return false;
}

void checkGameOver() {
    if (currentState == MULTIPLAYER) {
        if (currentPlayer == PLAYER_ONE) {
            if (!canMove(board)) {
                gameOver = true;
                if (gameOverP2) {
                    // Both players can't move, game is over
                    currentState = MULTIPLAYER_GAME_OVER;
                    playSound(gameoverSound);
                } else {
                    // Switch to player 2 if they can still move
                    currentPlayer = PLAYER_TWO;
                }
            }
        } else { // PLAYER_TWO
            if (!canMove(boardP2)) {
                gameOverP2 = true;
                if (gameOver) {
                    // Both players can't move, game is over
                    currentState = MULTIPLAYER_GAME_OVER;
                    playSound(gameoverSound);
                } else {
                    // Switch to player 1 if they can still move
                    currentPlayer = PLAYER_ONE;
                }
            }
        }
    } else {
        // Single player mode
        if (!canMove(board)) {
            currentState = GAME_OVER;
            playSound(gameoverSound);
        }
    }
}

void checkWin() {
    if (currentState == MULTIPLAYER) {
        if (currentPlayer == PLAYER_ONE) {
            for (int i = 0; i < BOARD_SIZE; i++) {
                for (int j = 0; j < BOARD_SIZE; j++) {
                    if (board[i][j] == 2048) {
                        won = true;
                        currentState = MULTIPLAYER_GAME_OVER;
                        playSound(gameoverSound);
                        return;
                    }
                }
            }
        } else { // PLAYER_TWO
            for (int i = 0; i < BOARD_SIZE; i++) {
                for (int j = 0; j < BOARD_SIZE; j++) {
                    if (boardP2[i][j] == 2048) {
                        wonP2 = true;
                        currentState = MULTIPLAYER_GAME_OVER;
                        playSound(gameoverSound);
                        return;
                    }
                }
            }
        }
    } else {
        // Single player mode
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] == 2048) {
                    won = true;
                    currentState = GAME_OVER;
                    playSound(gameoverSound);
                    return;
                }
            }
        }
    }
}

void savePreviousBoard() {
    // Save the current board state for animation
    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                previousBoardP2[i][j] = boardP2[i][j];
            }
        }
    } else {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                previousBoard[i][j] = board[i][j];
            }
        }
    }
}

// In the createMoveAnimations() function, modify to only create animations for tiles that actually move
void createMoveAnimations() {
    // Clear previous animations
    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
        animationsP2.clear();
        mergedTilesP2.clear();
    } else {
        animations.clear();
        mergedTiles.clear();
    }
    
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    std::vector<std::vector<int> >& prevBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? previousBoardP2 : previousBoard;
    std::vector<TileAnimation>& currentAnimations = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? animationsP2 : animations;
    std::map<std::pair<int, int>, std::pair<int, int>>& currentMergedTiles = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? mergedTilesP2 : mergedTiles;
    
    // Track which tiles in the previous board have been accounted for
    std::vector<std::vector<bool>> accounted(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    // First, identify merged tiles
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Skip empty cells in the current board
            if (currentBoard[i][j] == 0) continue;
            
            // Check if this is a merged tile (value is double of what was here before)
            if (prevBoard[i][j] != 0 && currentBoard[i][j] == prevBoard[i][j] * 2) {
                // This is a merged tile, we need to find where the other tile came from
                
                // First, mark this tile as accounted for
                accounted[i][j] = true;
                
                // Look for another tile with the same value in the previous board
                for (int ni = 0; ni < BOARD_SIZE; ni++) {
                    for (int nj = 0; nj < BOARD_SIZE; nj++) {
                        // Skip if already accounted for or not the same value
                        if (accounted[ni][nj] || prevBoard[ni][nj] != prevBoard[i][j]) continue;
                        
                        // Only create animation if the tile actually moved
                        if (ni != i || nj != j) {
                            // Create animation for the tile that moved to merge
                            TileAnimation anim;
                            anim.startRow = ni;
                            anim.startCol = nj;
                            anim.endRow = i;
                            anim.endCol = j;
                            anim.startTime = 0.0f;
                            anim.progress = 0.0f;
                            anim.state = MOVING;
                            anim.merged = true;
                            anim.value = prevBoard[ni][nj];
                            
                            currentAnimations.push_back(anim);
                            currentMergedTiles[std::make_pair(i, j)] = std::make_pair(ni, nj);
                        }
                        accounted[ni][nj] = true;
                        break;
                    }
                }
            }
        }
    }
    
    // Now identify moved tiles (not merged)
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Skip empty cells or already accounted for cells
            if (currentBoard[i][j] == 0) continue;
            
            // If this is not a merged tile, find where it came from
            if (currentMergedTiles.find(std::make_pair(i, j)) == currentMergedTiles.end()) {
                for (int pi = 0; pi < BOARD_SIZE; pi++) {
                    for (int pj = 0; pj < BOARD_SIZE; pj++) {
                        if (!accounted[pi][pj] && prevBoard[pi][pj] == currentBoard[i][j]) {
                            // Only create animation if the tile actually moved
                            if (pi != i || pj != j) {
                                // Create animation for the moved tile
                                TileAnimation anim;
                                anim.startRow = pi;
                                anim.startCol = pj;
                                anim.endRow = i;
                                anim.endCol = j;
                                anim.startTime = 0.0f;
                                anim.progress = 0.0f;
                                anim.state = MOVING;
                                anim.merged = false;
                                anim.value = prevBoard[pi][pj];
                                
                                currentAnimations.push_back(anim);
                            }
                            accounted[pi][pj] = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Start animation
    animating = !currentAnimations.empty();
    
    // Reset animation timer
    deltaTime = 0.0f;
    lastFrameTime = std::chrono::steady_clock::now();
    
    // Play move sound if there are animations
    if (animating) {
        playSound(moveSound);
    }
}

bool moveLeft() {
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    bool moved = false;
    bool merged = false;
    
    // Save the current board state for animation
    savePreviousBoard();
    
    // Clear merged tiles tracking
    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
        mergedTilesP2.clear();
    } else {
        mergedTiles.clear();
    }
    
    // Create a temporary board to track merged tiles
    std::vector<std::vector<bool>> mergedTracker(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        // Move all tiles to the left
        for (int j = 1; j < BOARD_SIZE; j++) {
            if (currentBoard[i][j] != 0) {
                int col = j;
                while (col > 0 && currentBoard[i][col - 1] == 0) {
                    currentBoard[i][col - 1] = currentBoard[i][col];
                    currentBoard[i][col] = 0;
                    col--;
                    moved = true;
                }
                
                // Check if we can merge with the tile to the left
                if (col > 0 && currentBoard[i][col - 1] == currentBoard[i][col] && !mergedTracker[i][col - 1]) {
                    currentBoard[i][col - 1] *= 2;
                    currentBoard[i][col] = 0;
                    mergedTracker[i][col - 1] = true;
                    
                    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
                        scoreP2 += currentBoard[i][col - 1];
                    } else {
                        score += currentBoard[i][col - 1];
                        if (score > bestScore) {
                            bestScore = score;
                        }
                    }
                    
                    moved = true;
                    merged = true;
                }
            }
        }
    }
    
    if (moved) {
        createMoveAnimations();
        boardTextureNeedsUpdate = true;
        
        // Play merge sound if any tiles were merged
        if (merged) {
            playSound(mergeSound);
        }
        
        // Lưu game sau mỗi lượt di chuyển
        saveGame();
    }
    
    return moved;
}

bool moveRight() {
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    bool moved = false;
    bool merged = false;
    
    // Save the current board state for animation
    savePreviousBoard();
    
    // Create a temporary board to track merged tiles
    std::vector<std::vector<bool>> mergedTracker(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        // Move all tiles to the right
        for (int j = BOARD_SIZE - 2; j >= 0; j--) {
            if (currentBoard[i][j] != 0) {
                int col = j;
                while (col < BOARD_SIZE - 1 && currentBoard[i][col + 1] == 0) {
                    currentBoard[i][col + 1] = currentBoard[i][col];
                    currentBoard[i][col] = 0;
                    col++;
                    moved = true;
                }
                
                // Check if we can merge with the tile to the right
                if (col < BOARD_SIZE - 1 && currentBoard[i][col + 1] == currentBoard[i][col] && !mergedTracker[i][col + 1]) {
                    currentBoard[i][col + 1] *= 2;
                    currentBoard[i][col] = 0;
                    mergedTracker[i][col + 1] = true;
                    
                    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
                        scoreP2 += currentBoard[i][col + 1];
                    } else {
                        score += currentBoard[i][col + 1];
                        if (score > bestScore) {
                            bestScore = score;
                        }
                    }
                    
                    moved = true;
                    merged = true;
                }
            }
        }
    }
    
    if (moved) {
        createMoveAnimations();
        boardTextureNeedsUpdate = true;
        
        // Play merge sound if any tiles were merged
        if (merged) {
            playSound(mergeSound);
        }
        
        // Lưu game sau mỗi lượt di chuyển
        saveGame();
    }
    
    return moved;
}

bool moveUp() {
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    bool moved = false;
    bool merged = false;
    
    // Save the current board state for animation
    savePreviousBoard();
    
    // Create a temporary board to track merged tiles
    std::vector<std::vector<bool>> mergedTracker(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    for (int j = 0; j < BOARD_SIZE; j++) {
        // Move all tiles up
        for (int i = 1; i < BOARD_SIZE; i++) {
            if (currentBoard[i][j] != 0) {
                int row = i;
                while (row > 0 && currentBoard[row - 1][j] == 0) {
                    currentBoard[row - 1][j] = currentBoard[row][j];
                    currentBoard[row][j] = 0;
                    row--;
                    moved = true;
                }
                
                // Check if we can merge with the tile above
                if (row > 0 && currentBoard[row - 1][j] == currentBoard[row][j] && !mergedTracker[row - 1][j]) {
                    currentBoard[row - 1][j] *= 2;
                    currentBoard[row][j] = 0;
                    mergedTracker[row - 1][j] = true;
                    
                    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
                        scoreP2 += currentBoard[row - 1][j];
                    } else {
                        score += currentBoard[row - 1][j];
                        if (score > bestScore) {
                            bestScore = score;
                        }
                    }
                    
                    moved = true;
                    merged = true;
                }
            }
        }
    }
    
    if (moved) {
        createMoveAnimations();
        boardTextureNeedsUpdate = true;
        
        // Play merge sound if any tiles were merged
        if (merged) {
            playSound(mergeSound);
        }
        
        // Lưu game sau mỗi lượt di chuyển
        saveGame();
    }
    
    return moved;
}

bool moveDown() {
    std::vector<std::vector<int> >& currentBoard = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? boardP2 : board;
    bool moved = false;
    bool merged = false;
    
    // Save the current board state for animation
    savePreviousBoard();
    
    // Create a temporary board to track merged tiles
    std::vector<std::vector<bool>> mergedTracker(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    for (int j = 0; j < BOARD_SIZE; j++) {
        // Move all tiles down
        for (int i = BOARD_SIZE - 2; i >= 0; i--) {
            if (currentBoard[i][j] != 0) {
                int row = i;
                while (row < BOARD_SIZE - 1 && currentBoard[row + 1][j] == 0) {
                    currentBoard[row + 1][j] = currentBoard[row][j];
                    currentBoard[row][j] = 0;
                    row++;
                    moved = true;
                }
                
                // Check if we can merge with the tile below
                if (row < BOARD_SIZE - 1 && currentBoard[row + 1][j] == currentBoard[row][j] && !mergedTracker[row + 1][j]) {
                    currentBoard[row + 1][j] *= 2;
                    currentBoard[row][j] = 0;
                    mergedTracker[row + 1][j] = true;
                    
                    if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
                        scoreP2 += currentBoard[row + 1][j];
                    } else {
                        score += currentBoard[row + 1][j];
                        if (score > bestScore) {
                            bestScore = score;
                        }
                    }
                    
                    moved = true;
                    merged = true;
                }
            }
        }
    }
    
    if (moved) {
        createMoveAnimations();
        boardTextureNeedsUpdate = true;
        
        // Play merge sound if any tiles were merged
        if (merged) {
            playSound(mergeSound);
        }
        
        // Lưu game sau mỗi lượt di chuyển
        saveGame();
    }
    
    return moved;
}

void updateBoardTexture() {
    if (!boardTextureNeedsUpdate) return;
    
    // Calculate board dimensions
    int boardWidth = BOARD_SIZE * TILE_SIZE + (BOARD_SIZE - 1) * TILE_MARGIN;
    int boardHeight = boardWidth;
    int boardX = (SCREEN_WIDTH - boardWidth) / 2;
    int boardY = HEADER_HEIGHT - 30;
    
    // Create or recreate the texture if needed
    if (boardTexture == nullptr) {
        boardTexture = SDL_CreateTexture(renderer, 
                                        SDL_PIXELFORMAT_RGBA8888, 
                                        SDL_TEXTUREACCESS_TARGET, 
                                        SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    
    // Save the current render target
    SDL_Texture* currentTarget = SDL_GetRenderTarget(renderer);
    
    // Set the board texture as the render target
    SDL_SetRenderTarget(renderer, boardTexture);
    
    // Clear the texture
    SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(renderer);
    
    // Draw board background
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    drawRoundedRect(renderer, boardX - TILE_MARGIN, boardY - TILE_MARGIN, 
                    boardWidth + TILE_MARGIN * 2, boardHeight + TILE_MARGIN * 2, 8);
    
    // Draw empty cells
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int x = boardX + j * (TILE_SIZE + TILE_MARGIN);
            int y = boardY + i * (TILE_SIZE + TILE_MARGIN);
            renderTile(0, x, y);
        }
    }
    
    // Draw static tiles (non-animated)
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != 0) {
                // Skip tiles that are being animated
                bool isAnimated = false;
                
                // Check if this tile is part of an animation
                for (const auto& anim : animations) {
                    if ((anim.endRow == i && anim.endCol == j) || 
                        (anim.startRow == i && anim.startCol == j && anim.state == MOVING)) {
                        isAnimated = true;
                        break;
                    }
                }
                
                // Check if this is a new tile
                for (const auto& newTile : newTiles) {
                    if (newTile.first == i && newTile.second == j) {
                        isAnimated = true;
                        break;
                    }
                }
                
                // Check if this is a merged tile
                if (mergedTiles.find(std::make_pair(i, j)) != mergedTiles.end()) {
                    isAnimated = true;
                }
                
                // Only render if not animated
                if (!isAnimated) {
                    int x = boardX + j * (TILE_SIZE + TILE_MARGIN);
                    int y = boardY + i * (TILE_SIZE + TILE_MARGIN);
                    renderTile(board[i][j], x, y);
                }
            }
        }
    }
    
    // Restore the original render target
    SDL_SetRenderTarget(renderer, currentTarget);
    
    boardTextureNeedsUpdate = false;
}

void renderAnimatedTile(int value, float x, float y, float scale) {
    // Set tile color based on value
    Color tileColor;
    if (value == 0) {
        tileColor = EMPTY_TILE_COLOR;
    } else {
        int colorIndex = std::min(static_cast<int>(log2(value)) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
        tileColor = TILE_COLORS[colorIndex];
    }
    
    // Calculate the size based on scale
    int size = static_cast<int>(TILE_SIZE * scale);
    int xOffset = static_cast<int>((TILE_SIZE - size) / 2);
    int yOffset = static_cast<int>((TILE_SIZE - size) / 2);
    
    // Draw rounded rectangle for the tile
    SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
    drawRoundedRect(renderer, static_cast<int>(x) + xOffset, static_cast<int>(y) + yOffset, size, size, static_cast<int>(6 * scale)); // Scale the corner radius
    
    // Don't render text for empty tiles
    if (value == 0) return;
    
    // Render tile value
    std::string valueStr = std::to_string(value);
    SDL_Color textColor = (value >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
    
    // Adjust font size based on number of digits
    TTF_Font* tileFont = font;
    if (value >= 1000) {
        tileFont = TTF_OpenFont(FONT_PATH, 24); // Smaller font for 4-digit numbers
        if (tileFont == nullptr) {
            tileFont = font; // Fallback to regular font
        }
    }
    
    SDL_Surface* textSurface = TTF_RenderText_Blended(tileFont, valueStr.c_str(), textColor);
    if (textSurface == nullptr) {
        // Handle error
        if (tileFont != font && tileFont != nullptr) {
            TTF_CloseFont(tileFont);
        }
        return;
    }
    
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    
    int textWidth = static_cast<int>(textSurface->w * scale);
    int textHeight = static_cast<int>(textSurface->h * scale);
    
    SDL_Rect textRect = {
        static_cast<int>(x + (TILE_SIZE - textWidth) / 2),
        static_cast<int>(y + (TILE_SIZE - textHeight) / 2),
        textWidth,
        textHeight
    };
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
    
    // Close the custom font if we created one
    if (tileFont != font && tileFont != nullptr) {
        TTF_CloseFont(tileFont);
    }
}

void renderTile(int value, int x, int y) {
    renderAnimatedTile(value, static_cast<float>(x), static_cast<float>(y), 1.0f);
}

void renderButton(const Button& button, TTF_Font* buttonFont) {
    // Render button background with rounded corners
    SDL_SetRenderDrawColor(renderer, 
                      button.isHovered ? BUTTON_HOVER_COLOR.r : BUTTON_COLOR.r,
                      button.isHovered ? BUTTON_HOVER_COLOR.g : BUTTON_COLOR.g,
                      button.isHovered ? BUTTON_HOVER_COLOR.b : BUTTON_COLOR.b,
                      255);
    drawRoundedRect(renderer, button.rect.x, button.rect.y, button.rect.w, button.rect.h, 10);
    
    // Render button text
    SDL_Color textColor = toSDLColor(LIGHT_TEXT);
    SDL_Surface* textSurface = TTF_RenderText_Blended(buttonFont, button.text.c_str(), textColor);
    if (textSurface == nullptr) {
        return; // Handle error
    }
    
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    
    SDL_Rect textRect = {
        button.rect.x + (button.rect.w - textSurface->w) / 2,
        button.rect.y + (button.rect.h - textSurface->h) / 2,
        textSurface->w,
        textSurface->h
    };
    
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

void renderGameUI() {
    // Render "Back" button in top left corner
    Button backButton;
    backButton.rect.x = BOARD_MARGIN;
    backButton.rect.y = BOARD_MARGIN;
    backButton.rect.w = 100;
    backButton.rect.h = 40;
    backButton.text = "Back";
    backButton.isHovered = isPointInRect(mouseX, mouseY, backButton.rect);
    renderButton(backButton, font);

    // Render "New Game" button next to "Back" button
    Button newGameButton;
    newGameButton.rect.x = BOARD_MARGIN + backButton.rect.w + 10;
    newGameButton.rect.y = BOARD_MARGIN;
    newGameButton.rect.w = 120;
    newGameButton.rect.h = 40;
    newGameButton.text = "New Game";
    newGameButton.isHovered = isPointInRect(mouseX, mouseY, newGameButton.rect);
    renderButton(newGameButton, font);

    // Render "2048" title
    SDL_Color textColor = toSDLColor(TEXT_COLOR);
    SDL_Surface* titleSurface = TTF_RenderText_Blended(titleFont, "2048", textColor);
    if (titleSurface != nullptr) {
        SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
        SDL_Rect titleRect = {
            BOARD_MARGIN,
            backButton.rect.y + backButton.rect.h + 10,
            titleSurface->w,
            titleSurface->h
        };
        SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
        SDL_FreeSurface(titleSurface);
        SDL_DestroyTexture(titleTexture);
    }

    // Render score boxes
    int boxWidth = 100;
    int boxHeight = 60;
    int boxMargin = 10;

    // Best score box
    static SDL_Texture* bestLabelTexture = nullptr;
    static SDL_Texture* bestScoreTexture = nullptr;
    static int lastBestScore = -1;
    static SDL_Rect bestLabelRect;
    static SDL_Rect bestScoreRect;
    
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    SDL_Rect bestScoreBox = {SCREEN_WIDTH - boxWidth - BOARD_MARGIN, BOARD_MARGIN, boxWidth, boxHeight};
    drawRoundedRect(renderer, bestScoreBox.x, bestScoreBox.y, bestScoreBox.w, bestScoreBox.h, 5);

    // Chỉ tạo texture mới khi cần thiết
    if (bestLabelTexture == nullptr) {
        SDL_Surface* bestLabelSurface = TTF_RenderText_Blended(menuFont, "BEST", textColor);
        if (bestLabelSurface != nullptr) {
            bestLabelTexture = SDL_CreateTextureFromSurface(renderer, bestLabelSurface);
            bestLabelRect = {
                bestScoreBox.x + (bestScoreBox.w - bestLabelSurface->w) / 2,
                bestScoreBox.y + 8,
                bestLabelSurface->w,
                bestLabelSurface->h
            };
            SDL_FreeSurface(bestLabelSurface);
        }
    }
    
    // Chỉ cập nhật texture điểm số khi điểm thay đổi
    if (lastBestScore != bestScore) {
        if (bestScoreTexture != nullptr) {
            SDL_DestroyTexture(bestScoreTexture);
            bestScoreTexture = nullptr;
        }
        
        std::string bestScoreStr = std::to_string(bestScore);
        SDL_Surface* bestScoreSurface = TTF_RenderText_Blended(font, bestScoreStr.c_str(), textColor);
        if (bestScoreSurface != nullptr) {
            bestScoreTexture = SDL_CreateTextureFromSurface(renderer, bestScoreSurface);
            bestScoreRect = {
                bestScoreBox.x + (bestScoreBox.w - bestScoreSurface->w) / 2,
                bestScoreBox.y + bestLabelRect.h + 5,
                bestScoreSurface->w,
                bestScoreSurface->h
            };
            SDL_FreeSurface(bestScoreSurface);
        }
        
        lastBestScore = bestScore;
    }
    
    // Render các texture đã tạo
    if (bestLabelTexture != nullptr) {
        SDL_RenderCopy(renderer, bestLabelTexture, NULL, &bestLabelRect);
    }
    
    if (bestScoreTexture != nullptr) {
        SDL_RenderCopy(renderer, bestScoreTexture, NULL, &bestScoreRect);
    }

    // Current score box
    static SDL_Texture* scoreLabelTexture = nullptr;
    static SDL_Texture* scoreTexture = nullptr;
    static int lastScore = -1;
    static SDL_Rect scoreLabelRect;
    static SDL_Rect scoreRect;
    
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    SDL_Rect scoreBox = {bestScoreBox.x - boxWidth - boxMargin, BOARD_MARGIN, boxWidth, boxHeight};
    drawRoundedRect(renderer, scoreBox.x, scoreBox.y, scoreBox.w, scoreBox.h, 5);

    // Chỉ tạo texture mới khi cần thiết
    if (scoreLabelTexture == nullptr) {
        SDL_Surface* scoreLabelSurface = TTF_RenderText_Blended(menuFont, "SCORE", textColor);
        if (scoreLabelSurface != nullptr) {
            scoreLabelTexture = SDL_CreateTextureFromSurface(renderer, scoreLabelSurface);
            scoreLabelRect = {
                scoreBox.x + (scoreBox.w - scoreLabelSurface->w) / 2,
                scoreBox.y + 8,
                scoreLabelSurface->w,
                scoreLabelSurface->h
            };
            SDL_FreeSurface(scoreLabelSurface);
        }
    }
    
    // Chỉ cập nhật texture điểm số khi điểm thay đổi
    if (lastScore != score) {
        if (scoreTexture != nullptr) {
            SDL_DestroyTexture(scoreTexture);
            scoreTexture = nullptr;
        }
        
        std::string scoreStr = std::to_string(score);
        SDL_Surface* scoreSurface = TTF_RenderText_Blended(font, scoreStr.c_str(), textColor);
        if (scoreSurface != nullptr) {
            scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);
            scoreRect = {
                scoreBox.x + (scoreBox.w - scoreSurface->w) / 2,
                scoreBox.y + scoreLabelRect.h + 5,
                scoreSurface->w,
                scoreSurface->h
            };
            SDL_FreeSurface(scoreSurface);
        }
        
        lastScore = score;
    }
    
    // Render các texture đã tạo
    if (scoreLabelTexture != nullptr) {
        SDL_RenderCopy(renderer, scoreLabelTexture, NULL, &scoreLabelRect);
    }
    
    if (scoreTexture != nullptr) {
        SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreRect);
    }
}

void renderMenu() {
    // Clear screen with menu background color
    SDL_SetRenderDrawColor(renderer, MENU_BACKGROUND_COLOR.r, MENU_BACKGROUND_COLOR.g, MENU_BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(renderer);
    
    // Render title
    SDL_Color titleColor = toSDLColor(TEXT_COLOR);
    SDL_Surface* titleSurface = TTF_RenderText_Blended(largeFont, "2048", titleColor);
    if (titleSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
    
    SDL_Rect titleRect = {
        (SCREEN_WIDTH - titleSurface->w) / 2,
        80,
        titleSurface->w,
        titleSurface->h
    };
    
    SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
    SDL_FreeSurface(titleSurface);
    SDL_DestroyTexture(titleTexture);
    
    // Render buttons
    for (size_t i = 0; i < menuButtons.size(); i++) {
        renderButton(menuButtons[i], menuFont);
    }
    
    // Update screen
    SDL_RenderPresent(renderer);
}

void renderHowToPlay() {
    // Clear screen with menu background color
    SDL_SetRenderDrawColor(renderer, MENU_BACKGROUND_COLOR.r, MENU_BACKGROUND_COLOR.g, MENU_BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(renderer);
    
    // Render title
    SDL_Color textColor = toSDLColor(TEXT_COLOR);
    SDL_Surface* titleSurface = TTF_RenderText_Blended(titleFont, "How to Play", textColor);
    if (titleSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
    
    SDL_Rect titleRect = {
        (SCREEN_WIDTH - titleSurface->w) / 2,
        50,
        titleSurface->w,
        titleSurface->h
    };
    
    SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
    SDL_FreeSurface(titleSurface);
    SDL_DestroyTexture(titleTexture);
    
    // Render instructions
    const char* instructions[] = {
        "Single Player: Use arrow keys or WASD to move tiles",
        "Tiles with the same number merge into one when they touch",
        "A new tile appears after each move",
        "Try to create a tile with the number 2048!",
        "Press R to restart the game at any time",
        "In multiplayer mode:",
        "Player 1 (left) uses WASD keys",
        "Player 2 (right) uses arrow keys",
        "The player who reaches 2048 first or has the highest",
        "score when no more moves are possible wins!",
        ""
    
    };
    
    for (int i = 0; i < 11; i++) {
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, instructions[i], textColor);
        if (textSurface == nullptr) continue; // Skip if error
        
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        
        SDL_Rect textRect = {
            (SCREEN_WIDTH - textSurface->w) / 2,
            150 + i * 40,
            textSurface->w,
            textSurface->h
        };
        
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }
    
    // Render back button
    for (size_t i = 0; i < howToPlayButtons.size(); i++) {
        renderButton(howToPlayButtons[i], menuFont);
    }
    
    // Update screen
    SDL_RenderPresent(renderer);
}

void renderGameBoard() {
    // Clear screen
    SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(renderer);
    
    // Update the board texture if needed
    if (!animating || boardTextureNeedsUpdate) {
        updateBoardTexture();
    }
    
    // Render the board texture
    SDL_RenderCopy(renderer, boardTexture, NULL, NULL);
    
    // Calculate board position
    int boardWidth = BOARD_SIZE * TILE_SIZE + (BOARD_SIZE - 1) * TILE_MARGIN;
    int boardHeight = boardWidth;
    int boardX = (SCREEN_WIDTH - boardWidth) / 2;
    int boardY = HEADER_HEIGHT - 30;
    
    // Create a map to track which cells have animated tiles
    std::vector<std::vector<bool>> cellAnimated(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    // Render animated tiles on top
    if (animating) {
        // Render moving tiles
        for (const auto& anim : animations) {
            if (anim.state == MOVING) {
                // Calculate interpolated position
                float progress = std::min(1.0f, anim.progress / ANIMATION_DURATION);
                float eased = easeInOut(progress);
                
                float startX = static_cast<float>(boardX + anim.startCol * (TILE_SIZE + TILE_MARGIN));
                float startY = static_cast<float>(boardY + anim.startRow * (TILE_SIZE + TILE_MARGIN));
                float endX = static_cast<float>(boardX + anim.endCol * (TILE_SIZE + TILE_MARGIN));
                float endY = static_cast<float>(boardY + anim.endRow * (TILE_SIZE + TILE_MARGIN));
                
                // Để tránh di chuyển chéo, di chuyển theo hai bước: ngang trước, dọc sau
                float x, y;
                
                // Nếu cùng hàng, chỉ di chuyển ngang
                if (anim.startRow == anim.endRow) {
                    x = lerp(startX, endX, eased);
                    y = startY; // Giữ nguyên y
                }
                // Nếu cùng cột, chỉ di chuyển dọc
                else if (anim.startCol == anim.endCol) {
                    x = startX; // Giữ nguyên x
                    y = lerp(startY, endY, eased);
                }
                // Nếu khác cả hàng và cột (trường hợp hiếm gặp), di chuyển ngang trước, dọc sau
                else {
                    if (progress < 0.5f) {
                        // Di chuyển ngang trước
                        x = lerp(startX, endX, eased * 2.0f);
                        y = startY;
                    } else {
                        // Di chuyển dọc sau
                        x = endX;
                        y = lerp(startY, endY, (eased - 0.5f) * 2.0f);
                    }
                }
                
                renderAnimatedTile(anim.value, x, y, 1.0f);
                
                // Mark both source and destination cells as animated
                cellAnimated[anim.startRow][anim.startCol] = true;
                cellAnimated[anim.endRow][anim.endCol] = true;
            }
        }
        
        // Render merged tiles
        for (const auto& [pos, srcPos] : mergedTiles) {
            int i = pos.first;
            int j = pos.second;
            int x = boardX + j * (TILE_SIZE + TILE_MARGIN);
            int y = boardY + i * (TILE_SIZE + TILE_MARGIN);
            
            // Calculate animation progress
            float maxTime = ANIMATION_DURATION + MERGE_ANIMATION_DURATION;
            float progress = deltaTime / maxTime;
            
            if (progress > ANIMATION_DURATION / maxTime) {
                // Calculate merge animation progress
                float mergeProgress = (progress - ANIMATION_DURATION / maxTime) / (MERGE_ANIMATION_DURATION / maxTime);
                mergeProgress = std::min(1.0f, mergeProgress);
                
                // Scale animation - more like original 2048
                float scale;
                if (mergeProgress < 0.5f) {
                    // Grow phase - faster growth to 1.2x
                    scale = lerp(1.0f, 1.2f, mergeProgress * 2.0f);
                } else {
                    // Shrink phase - smooth return to normal size
                    scale = lerp(1.2f, 1.0f, (mergeProgress - 0.5f) * 2.0f);
                }
                
                renderAnimatedTile(board[i][j], static_cast<float>(x), static_cast<float>(y), scale);
                
                // Mark the cell as animated
                cellAnimated[i][j] = true;
            }
        }
        
        // Render new tiles with pop-up animation
        for (const auto& [row, col] : newTiles) {
            int x = boardX + col * (TILE_SIZE + TILE_MARGIN);
            int y = boardY + row * (TILE_SIZE + TILE_MARGIN);
            
            // Calculate scale for appear animation - start from 0 and grow with slight bounce
            float progress = (deltaTime - ANIMATION_DURATION - NEW_TILE_DELAY) / NEW_TILE_ANIMATION_DURATION;
            progress = std::max(0.0f, std::min(1.0f, progress));
            
            float scale = 0.0f;
            if (progress > 0.0f) {
                // Simple grow animation from 0 to 1 with slight overshoot
                if (progress < 0.7f) {
                    // Grow from 0 to 1.05 (slight overshoot)
                    scale = progress / 0.7f * 1.05f;
                } else {
                    // Settle back from 1.05 to 1.0
                    scale = 1.05f - ((progress - 0.7f) / 0.3f * 0.05f);
                }
                
                // Center the tile as it grows
                float centerX = x + TILE_SIZE / 2.0f;
                float centerY = y + TILE_SIZE / 2.0f;
                float scaledX = centerX - (TILE_SIZE * scale) / 2.0f;
                float scaledY = centerY - (TILE_SIZE * scale) / 2.0f;
                
                renderAnimatedTile(board[row][col], scaledX, scaledY, scale);
                
                // Mark the cell as animated
                cellAnimated[row][col] = true;
            }
        }
        
        // Render static tiles (tiles that don't move)
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] != 0 && !cellAnimated[i][j]) {
                    int x = boardX + j * (TILE_SIZE + TILE_MARGIN);
                    int y = boardY + i * (TILE_SIZE + TILE_MARGIN);
                    renderTile(board[i][j], x, y);
                }
            }
        }
    }
    
    // Render UI elements (buttons, scores, etc.)
    renderGameUI();
    
    // Present the renderer
    SDL_RenderPresent(renderer);
}

void renderMultiplayerGameBoard() {
    // Clear screen with background color (light cream)
    SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(renderer);

    // Render "Back" button in top left corner
    Button backButton;
    backButton.rect.x = BOARD_MARGIN;
    backButton.rect.y = BOARD_MARGIN;
    backButton.rect.w = 100;
    backButton.rect.h = 40;
    backButton.text = "Back";
    backButton.isHovered = isPointInRect(mouseX, mouseY, backButton.rect);
    renderButton(backButton, font);

    // Render "New Game" button next to "Back" button
    Button newGameButton;
    newGameButton.rect.x = BOARD_MARGIN + backButton.rect.w + 10; // 10px spacing between buttons
    newGameButton.rect.y = BOARD_MARGIN;
    newGameButton.rect.w = 120;
    newGameButton.rect.h = 40;
    newGameButton.text = "New Game";
    newGameButton.isHovered = isPointInRect(mouseX, mouseY, newGameButton.rect);
    renderButton(newGameButton, font);

    // Render "2048" title in the top right corner
    SDL_Color textColor = toSDLColor(TEXT_COLOR);
    SDL_Surface* titleSurface = TTF_RenderText_Blended(titleFont, "2048", textColor);
    if (titleSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);

    SDL_Rect titleRect = {
        SCREEN_WIDTH - titleSurface->w - BOARD_MARGIN,
        BOARD_MARGIN,
        titleSurface->w,
        titleSurface->h
    };

    SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
    SDL_FreeSurface(titleSurface);
    SDL_DestroyTexture(titleTexture);

    // Calculate board positions - two boards side by side with full size
    int boardWidth = static_cast<int>((BOARD_SIZE * TILE_SIZE + (BOARD_SIZE - 1) * TILE_MARGIN) * 0.85); // 85% of original size
    int boardHeight = boardWidth;
    int tileSize = static_cast<int>(TILE_SIZE * 0.85);
    int tileMargin = static_cast<int>(TILE_MARGIN * 0.85);
    int boardSpacing = 60; // Reduced space between boards
    
    int boardP1X = (SCREEN_WIDTH / 2) - boardWidth - (boardSpacing / 2);
    int boardP2X = (SCREEN_WIDTH / 2) + (boardSpacing / 2);
    int boardY = HEADER_HEIGHT;

    // Draw player 1 board background with rounded corners - using same color as single player
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    drawRoundedRect(renderer, boardP1X - tileMargin, boardY - tileMargin, 
                    boardWidth + tileMargin * 2, boardHeight + tileMargin * 2, 8);

    // Draw player 2 board background with rounded corners - using same color as single player
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    drawRoundedRect(renderer, boardP2X - tileMargin, boardY - tileMargin, 
                    boardWidth + tileMargin * 2, boardHeight + tileMargin * 2, 8);

    // Render empty cells for player 1 board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int x = boardP1X + j * (tileSize + tileMargin);
            int y = boardY + i * (tileSize + tileMargin);
            
            // Draw empty tile
            SDL_SetRenderDrawColor(renderer, EMPTY_TILE_COLOR.r, EMPTY_TILE_COLOR.g, EMPTY_TILE_COLOR.b, 255);
            drawRoundedRect(renderer, x, y, tileSize, tileSize, 6);
        }
    }
    
    // Render empty cells for player 2 board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int x = boardP2X + j * (tileSize + tileMargin);
            int y = boardY + i * (tileSize + tileMargin);
            
            // Draw empty tile
            SDL_SetRenderDrawColor(renderer, EMPTY_TILE_COLOR.r, EMPTY_TILE_COLOR.g, EMPTY_TILE_COLOR.b, 255);
            drawRoundedRect(renderer, x, y, tileSize, tileSize, 6);
        }
    }
    
    // Create a map to track which cells have animated tiles for each player
    std::vector<std::vector<bool>> cellOccupiedP1(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    std::vector<std::vector<bool>> cellOccupiedP2(BOARD_SIZE, std::vector<bool>(BOARD_SIZE, false));
    
    // Render animated tiles for player 1
    if (animating && currentPlayer == PLAYER_ONE) {
        // First render all non-merged tiles that are moving
        for (size_t i = 0; i < animations.size(); i++) {
            TileAnimation& anim = animations[i];
            
            if (anim.state == MOVING && !anim.merged) {
                // Calculate interpolated position with improved easing
                float progress = std::min(1.0f, anim.progress / ANIMATION_DURATION);
                float eased = easeInOut(progress);
                
                float startX = static_cast<float>(boardP1X + anim.startCol * (tileSize + tileMargin));
                float startY = static_cast<float>(boardY + anim.startRow * (tileSize + tileMargin));
                float endX = static_cast<float>(boardP1X + anim.endCol * (tileSize + tileMargin));
                float endY = static_cast<float>(boardY + anim.endRow * (tileSize + tileMargin));
                
                // Để tránh di chuyển chéo, di chuyển theo hai bước: ngang trước, dọc sau
                float x, y;
                
                // Nếu cùng hàng, chỉ di chuyển ngang
                if (anim.startRow == anim.endRow) {
                    x = lerp(startX, endX, eased);
                    y = startY; // Giữ nguyên y
                }
                // Nếu cùng cột, chỉ di chuyển dọc
                else if (anim.startCol == anim.endCol) {
                    x = startX; // Giữ nguyên x
                    y = lerp(startY, endY, eased);
                }
                // Nếu khác cả hàng và cột (trường hợp hiếm gặp), di chuyển ngang trước, dọc sau
                else {
                    if (progress < 0.5f) {
                        // Di chuyển ngang trước
                        x = lerp(startX, endX, eased * 2.0f);
                        y = startY;
                    } else {
                        // Di chuyển dọc sau
                        x = endX;
                        y = lerp(startY, endY, (eased - 0.5f) * 2.0f);
                    }
                }
                
                // Set tile color based on value
                Color tileColor;
                if (anim.value == 0) {
                    tileColor = EMPTY_TILE_COLOR;
                } else {
                    int colorIndex = std::min(static_cast<int>(log2(anim.value)) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                    tileColor = TILE_COLORS[colorIndex];
                }
                
                // Draw tile
                SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                drawRoundedRect(renderer, static_cast<int>(x), static_cast<int>(y), tileSize, tileSize, 6);
                
                // Render tile value
                if (anim.value > 0) {
                    std::string valueStr = std::to_string(anim.value);
                    SDL_Color textColor = (anim.value >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                    
                    SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                    if (textSurface == nullptr) continue; // Skip if error
                    
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    
                    SDL_Rect textRect = {
                        static_cast<int>(x + (tileSize - textSurface->w) / 2),
                        static_cast<int>(y + (tileSize - textSurface->h) / 2),
                        textSurface->w,
                        textSurface->h
                    };
                    
                    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                    SDL_FreeSurface(textSurface);
                    SDL_DestroyTexture(textTexture);
                }
                
                // Mark destination cell as occupied
                if (progress >= 0.99f) {
                    cellOccupiedP1[anim.endRow][anim.endCol] = true;
                }
            }
        }
        
        // Then render merged tiles with separate animations
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                std::pair<int, int> pos = std::make_pair(i, j);
                if (mergedTiles.find(pos) != mergedTiles.end()) {
                    int x = boardP1X + j * (tileSize + tileMargin);
                    int y = boardY + i * (tileSize + tileMargin);
                    
                    // Calculate animation progress
                    float maxTime = ANIMATION_DURATION + MERGE_ANIMATION_DURATION;
                    float progress = deltaTime / maxTime;
                    
                    // If we're in the merge phase (after movement)
                    if (progress > ANIMATION_DURATION / maxTime) {
                        // Calculate merge animation progress
                        float mergeProgress = (progress - ANIMATION_DURATION / maxTime) / (MERGE_ANIMATION_DURATION / maxTime);
                        mergeProgress = std::min(1.0f, mergeProgress);
                        
                        // Scale animation - more like original 2048
                        float scale;
                        if (mergeProgress < 0.5f) {
                            // Grow phase - faster growth to 1.2x
                            scale = lerp(1.0f, 1.2f, mergeProgress * 2.0f);
                        } else {
                            // Shrink phase - smooth return to normal size
                            scale = lerp(1.2f, 1.0f, (mergeProgress - 0.5f) * 2.0f);
                        }
                        
                        // Calculate size based on scale
                        int scaledSize = static_cast<int>(tileSize * scale);
                        int xOffset = static_cast<int>((tileSize - scaledSize) / 2);
                        int yOffset = static_cast<int>((tileSize - scaledSize) / 2);
                        
                        // Set tile color
                        Color tileColor;
                        int colorIndex = std::min(static_cast<int>(log2(board[i][j])) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                        tileColor = TILE_COLORS[colorIndex];
                        
                        // Draw tile
                        SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                        drawRoundedRect(renderer, x + xOffset, y + yOffset, scaledSize, scaledSize, static_cast<int>(6 * scale));
                        
                        // Render tile value
                        std::string valueStr = std::to_string(board[i][j]);
                        SDL_Color textColor = (board[i][j] >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                        
                        SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                        if (textSurface == nullptr) continue; // Skip if error
                        
                        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                        
                        int textWidth = static_cast<int>(textSurface->w * scale);
                        int textHeight = static_cast<int>(textSurface->h * scale);
                        
                        SDL_Rect textRect = {
                            static_cast<int>(x + (tileSize - textWidth) / 2),
                            static_cast<int>(y + (tileSize - textHeight) / 2),
                            textWidth,
                            textHeight
                        };
                        
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_FreeSurface(textSurface);
                        SDL_DestroyTexture(textTexture);
                        
                        cellOccupiedP1[i][j] = true;
                    }
                }
            }
        }
        
        // Render new tiles with pop-up animation
        for (size_t i = 0; i < newTiles.size(); i++) {
            int row = newTiles[i].first;
            int col = newTiles[i].second;
            int x = boardP1X + col * (tileSize + tileMargin);
            int y = boardY + row * (tileSize + tileMargin);
            
            // Calculate scale for appear animation - start from 0 and grow with slight bounce
            float progress = (deltaTime - ANIMATION_DURATION - NEW_TILE_DELAY) / NEW_TILE_ANIMATION_DURATION;
            progress = std::max(0.0f, std::min(1.0f, progress));
            
            float scale = 0.0f;
            if (progress > 0.0f) {
                if (progress < 0.7f) {
                    // Grow from 0 to 1.05 (slight overshoot)
                    scale = progress / 0.7f * 1.05f;
                } else {
                    // Settle back from 1.05 to 1.0
                    scale = 1.05f - ((progress - 0.7f) / 0.3f * 0.05f);
                }
                
                // Center the tile as it grows
                float centerX = x + tileSize / 2.0f;
                float centerY = y + tileSize / 2.0f;
                float scaledX = centerX - (tileSize * scale) / 2.0f;
                float scaledY = centerY - (tileSize * scale) / 2.0f;
                
                // Set tile color
                Color tileColor;
                int colorIndex = std::min(static_cast<int>(log2(board[row][col])) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                tileColor = TILE_COLORS[colorIndex];
                
                // Draw tile
                SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                drawRoundedRect(renderer, static_cast<int>(scaledX), static_cast<int>(scaledY), 
                               static_cast<int>(tileSize * scale), static_cast<int>(tileSize * scale), 
                               static_cast<int>(6 * scale));
                
                // Render tile value
                std::string valueStr = std::to_string(board[row][col]);
                SDL_Color textColor = (board[row][col] >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                
                SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                if (textSurface == nullptr) continue; // Skip if error
                
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                
                int textWidth = static_cast<int>(textSurface->w * scale);
                int textHeight = static_cast<int>(textSurface->h * scale);
                
                SDL_Rect textRect = {
                    static_cast<int>(scaledX + (tileSize * scale - textWidth) / 2),
                    static_cast<int>(scaledY + (tileSize * scale - textHeight) / 2),
                    textWidth,
                    textHeight
                };
                
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_FreeSurface(textSurface);
                SDL_DestroyTexture(textTexture);
                
                cellOccupiedP1[row][col] = true;
            }
        }
    }
    
    // Render player 1 board without animations or static tiles
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != 0 && !cellOccupiedP1[i][j]) {
                int x = boardP1X + j * (tileSize + tileMargin);
                int y = boardY + i * (tileSize + tileMargin);
                
                // Set tile color based on value
                Color tileColor;
                int colorIndex = std::min(static_cast<int>(log2(board[i][j])) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                tileColor = TILE_COLORS[colorIndex];
                
                // Draw tile
                SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                drawRoundedRect(renderer, x, y, tileSize, tileSize, 6);
                
                // Render tile value
                std::string valueStr = std::to_string(board[i][j]);
                SDL_Color textColor = (board[i][j] >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                
                SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                if (textSurface == nullptr) continue; // Skip if error
                
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                
                SDL_Rect textRect = {
                    x + (tileSize - textSurface->w) / 2,
                    y + (tileSize - textSurface->h) / 2,
                    textSurface->w,
                    textSurface->h
                };
                
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_FreeSurface(textSurface);
                SDL_DestroyTexture(textTexture);
            }
        }
    }
    
    // Render animated tiles for player 2
    if (animating && currentPlayer == PLAYER_TWO) {
        // First render all non-merged tiles that are moving
        for (size_t i = 0; i < animationsP2.size(); i++) {
            TileAnimation& anim = animationsP2[i];
            
            if (anim.state == MOVING && !anim.merged) {
                // Calculate interpolated position with improved easing
                float progress = std::min(1.0f, anim.progress / ANIMATION_DURATION);
                float eased = easeInOut(progress);
                
                float startX = static_cast<float>(boardP2X + anim.startCol * (tileSize + tileMargin));
                float startY = static_cast<float>(boardY + anim.startRow * (tileSize + tileMargin));
                float endX = static_cast<float>(boardP2X + anim.endCol * (tileSize + tileMargin));
                float endY = static_cast<float>(boardY + anim.endRow * (tileSize + tileMargin));
                
                // Để tránh di chuyển chéo, di chuyển theo hai bước: ngang trước, dọc sau
                float x, y;
                
                // Nếu cùng hàng, chỉ di chuyển ngang
                if (anim.startRow == anim.endRow) {
                    x = lerp(startX, endX, eased);
                    y = startY; // Giữ nguyên y
                }
                // Nếu cùng cột, chỉ di chuyển dọc
                else if (anim.startCol == anim.endCol) {
                    x = startX; // Giữ nguyên x
                    y = lerp(startY, endY, eased);
                }
                // Nếu khác cả hàng và cột (trường hợp hiếm gặp), di chuyển ngang trước, dọc sau
                else {
                    if (progress < 0.5f) {
                        // Di chuyển ngang trước
                        x = lerp(startX, endX, eased * 2.0f);
                        y = startY;
                    } else {
                        // Di chuyển dọc sau
                        x = endX;
                        y = lerp(startY, endY, (eased - 0.5f) * 2.0f);
                    }
                }
                
                // Set tile color based on value
                Color tileColor;
                if (anim.value == 0) {
                    tileColor = EMPTY_TILE_COLOR;
                } else {
                    int colorIndex = std::min(static_cast<int>(log2(anim.value)) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                    tileColor = TILE_COLORS[colorIndex];
                }
                
                // Draw tile
                SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                drawRoundedRect(renderer, static_cast<int>(x), static_cast<int>(y), tileSize, tileSize, 6);
                
                // Render tile value
                if (anim.value > 0) {
                    std::string valueStr = std::to_string(anim.value);
                    SDL_Color textColor = (anim.value >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                    
                    SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                    if (textSurface == nullptr) continue; // Skip if error
                    
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    
                    SDL_Rect textRect = {
                        static_cast<int>(x + (tileSize - textSurface->w) / 2),
                        static_cast<int>(y + (tileSize - textSurface->h) / 2),
                        textSurface->w,
                        textSurface->h
                    };
                    
                    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                    SDL_FreeSurface(textSurface);
                    SDL_DestroyTexture(textTexture);
                }
                
                // Mark destination cell as occupied
                if (progress >= 0.99f) {
                    cellOccupiedP2[anim.endRow][anim.endCol] = true;
                }
            }
        }
        
        // Then render merged tiles with separate animations
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                std::pair<int, int> pos = std::make_pair(i, j);
                if (mergedTilesP2.find(pos) != mergedTilesP2.end()) {
                    int x = boardP2X + j * (tileSize + tileMargin);
                    int y = boardY + i * (tileSize + tileMargin);
                    
                    // Calculate animation progress
                    float maxTime = ANIMATION_DURATION + MERGE_ANIMATION_DURATION;
                    float progress = deltaTime / maxTime;
                    
                    // If we're in the merge phase (after movement)
                    if (progress > ANIMATION_DURATION / maxTime) {
                        // Calculate merge animation progress
                        float mergeProgress = (progress - ANIMATION_DURATION / maxTime) / (MERGE_ANIMATION_DURATION / maxTime);
                        mergeProgress = std::min(1.0f, mergeProgress);
                        
                        // Scale animation - more like original 2048
                        float scale;
                        if (mergeProgress < 0.5f) {
                            // Grow phase - faster growth to 1.2x
                            scale = lerp(1.0f, 1.2f, mergeProgress * 2.0f);
                        } else {
                            // Shrink phase - smooth return to normal size
                            scale = lerp(1.2f, 1.0f, (mergeProgress - 0.5f) * 2.0f);
                        }
                        
                        // Calculate size based on scale
                        int scaledSize = static_cast<int>(tileSize * scale);
                        int xOffset = static_cast<int>((tileSize - scaledSize) / 2);
                        int yOffset = static_cast<int>((tileSize - scaledSize) / 2);
                        
                        // Set tile color
                        Color tileColor;
                        int colorIndex = std::min(static_cast<int>(log2(boardP2[i][j])) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                        tileColor = TILE_COLORS[colorIndex];
                        
                        // Draw tile
                        SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                        drawRoundedRect(renderer, x + xOffset, y + yOffset, scaledSize, scaledSize, static_cast<int>(6 * scale));
                        
                        // Render tile value
                        std::string valueStr = std::to_string(boardP2[i][j]);
                        SDL_Color textColor = (boardP2[i][j] >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                        
                        SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                        if (textSurface == nullptr) continue; // Skip if error
                        
                        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                        
                        int textWidth = static_cast<int>(textSurface->w * scale);
                        int textHeight = static_cast<int>(textSurface->h * scale);
                        
                        SDL_Rect textRect = {
                            static_cast<int>(x + (tileSize - textWidth) / 2),
                            static_cast<int>(y + (tileSize - textHeight) / 2),
                            textWidth,
                            textHeight
                        };
                        
SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_FreeSurface(textSurface);
                        SDL_DestroyTexture(textTexture);
                        
                        cellOccupiedP2[i][j] = true;
                    }
                }
            }
        }
    }
    
    // Render player 2 board without animations
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (boardP2[i][j] != 0 && !cellOccupiedP2[i][j]) {
                int x = boardP2X + j * (tileSize + tileMargin);
                int y = boardY + i * (tileSize + tileMargin);
                
                // Set tile color based on value
                Color tileColor;
                int colorIndex = std::min(static_cast<int>(log2(boardP2[i][j])) - 1, static_cast<int>(TILE_COLORS.size()) - 1);
                tileColor = TILE_COLORS[colorIndex];
                
                // Draw tile
                SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, 255);
                drawRoundedRect(renderer, x, y, tileSize, tileSize, 6);
                
                // Render tile value
                std::string valueStr = std::to_string(boardP2[i][j]);
                SDL_Color textColor = (boardP2[i][j] >= 8) ? toSDLColor(LIGHT_TEXT) : toSDLColor(TEXT_COLOR);
                
                SDL_Surface* textSurface = TTF_RenderText_Blended(menuFont, valueStr.c_str(), textColor);
                if (textSurface == nullptr) continue; // Skip if error
                
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                
                SDL_Rect textRect = {
                    x + (tileSize - textSurface->w) / 2,
                    y + (tileSize - textSurface->h) / 2,
                    textSurface->w,
                    textSurface->h
                };
                
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                SDL_FreeSurface(textSurface);
                SDL_DestroyTexture(textTexture);
            }
        }
    }

    // Player 1 header with score - moved below the board
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    SDL_Rect p1Header = {boardP1X, boardY + boardHeight + 15, boardWidth, 40};
    drawRoundedRect(renderer, p1Header.x, p1Header.y, p1Header.w, p1Header.h, 5);
    
    // Player 1 label
    SDL_Surface* p1LabelSurface = TTF_RenderText_Blended(menuFont, "Player 1 (WASD)", textColor);
    if (p1LabelSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p1LabelTexture = SDL_CreateTextureFromSurface(renderer, p1LabelSurface);
    SDL_Rect p1LabelRect = {
        p1Header.x + 10,
        p1Header.y + (p1Header.h - p1LabelSurface->h) / 2,
        p1LabelSurface->w,
        p1LabelSurface->h
    };
    SDL_RenderCopy(renderer, p1LabelTexture, NULL, &p1LabelRect);
    SDL_FreeSurface(p1LabelSurface);
    SDL_DestroyTexture(p1LabelTexture);
    
    // Player 1 score
    std::string p1ScoreStr = "Score: " + std::to_string(score);
    SDL_Surface* p1ScoreSurface = TTF_RenderText_Blended(menuFont, p1ScoreStr.c_str(), textColor);
    if (p1ScoreSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p1ScoreTexture = SDL_CreateTextureFromSurface(renderer, p1ScoreSurface);
    SDL_Rect p1ScoreRect = {
        p1Header.x + p1Header.w - p1ScoreSurface->w - 10,
        p1Header.y + (p1Header.h - p1ScoreSurface->h) / 2,
        p1ScoreSurface->w,
        p1ScoreSurface->h
    };
    SDL_RenderCopy(renderer, p1ScoreTexture, NULL, &p1ScoreRect);
    SDL_FreeSurface(p1ScoreSurface);
    SDL_DestroyTexture(p1ScoreTexture);
    
    // Player 2 header with score - moved below the board
    SDL_SetRenderDrawColor(renderer, BOARD_COLOR.r, BOARD_COLOR.g, BOARD_COLOR.b, 255);
    SDL_Rect p2Header = {boardP2X, boardY + boardHeight + 15, boardWidth, 40};
    drawRoundedRect(renderer, p2Header.x, p2Header.y, p2Header.w, p2Header.h, 5);
    
    // Player 2 label
    SDL_Surface* p2LabelSurface = TTF_RenderText_Blended(menuFont, "Player 2 (Arrows)", textColor);
    if (p2LabelSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p2LabelTexture = SDL_CreateTextureFromSurface(renderer, p2LabelSurface);
    SDL_Rect p2LabelRect = {
        p2Header.x + 10,
        p2Header.y + (p2Header.h - p2LabelSurface->h) / 2,
        p2LabelSurface->w,
        p2LabelSurface->h
    };
    SDL_RenderCopy(renderer, p2LabelTexture, NULL, &p2LabelRect);
    SDL_FreeSurface(p2LabelSurface);
    SDL_DestroyTexture(p2LabelTexture);
    
    // Player 2 score
    std::string p2ScoreStr = "Score: " + std::to_string(scoreP2);
    SDL_Surface* p2ScoreSurface = TTF_RenderText_Blended(menuFont, p2ScoreStr.c_str(), textColor);
    if (p2ScoreSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p2ScoreTexture = SDL_CreateTextureFromSurface(renderer, p2ScoreSurface);
    SDL_Rect p2ScoreRect = {
        p2Header.x + p2Header.w - p2ScoreSurface->w - 10,
        p2Header.y + (p2Header.h - p2ScoreSurface->h) / 2,
        p2ScoreSurface->w,
        p2ScoreSurface->h
    };
    SDL_RenderCopy(renderer, p2ScoreTexture, NULL, &p2ScoreRect);
    SDL_FreeSurface(p2ScoreSurface);
    SDL_DestroyTexture(p2ScoreTexture);
    
    // Update screen
    SDL_RenderPresent(renderer);
}

void renderGameOver() {
    // Create semi-transparent overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);
    
    // Render message
    std::string message = won ? "You Win!" : "Game Over!";
    SDL_Color whiteColor;
    whiteColor.r = 255;
    whiteColor.g = 255;
    whiteColor.b = 255;
    whiteColor.a = 255;
    
    SDL_Surface* messageSurface = TTF_RenderText_Blended(largeFont, message.c_str(), whiteColor);
    if (messageSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);
    
    SDL_Rect messageRect = {
        (SCREEN_WIDTH - messageSurface->w) / 2,
        200,
        messageSurface->w,
        messageSurface->h
    };
    
    SDL_RenderCopy(renderer, messageTexture, NULL, &messageRect);
    SDL_FreeSurface(messageSurface);
    SDL_DestroyTexture(messageTexture);
    
    // Render final score
    std::string scoreStr = "Score: " + std::to_string(score);
    SDL_Surface* scoreSurface = TTF_RenderText_Blended(titleFont, scoreStr.c_str(), whiteColor);
    if (scoreSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);
    
    SDL_Rect scoreRect = {
        (SCREEN_WIDTH - scoreSurface->w) / 2,
        280,
        scoreSurface->w,
        scoreSurface->h
    };
    
    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreRect);
    SDL_FreeSurface(scoreSurface);
    SDL_DestroyTexture(scoreTexture);
    
    // Render buttons
    for (size_t i = 0; i < gameOverButtons.size(); i++) {
        renderButton(gameOverButtons[i], menuFont);
    }
    
    // Update screen
    SDL_RenderPresent(renderer);
}

void renderMultiplayerGameOver() {
    // Create semi-transparent overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);
    
    // Determine winner
    std::string message;
    if (won) {
        message = "Player 1 Wins!";
    } else if (wonP2) {
        message = "Player 2 Wins!";
    } else if (score > scoreP2) {
        message = "Player 1 Wins!";
    } else if (scoreP2 > score) {
        message = "Player 2 Wins!";
    } else {
        message = "It's a Tie!";
    }
    
    SDL_Color whiteColor;
    whiteColor.r = 255;
    whiteColor.g = 255;
    whiteColor.b = 255;
    whiteColor.a = 255;
    
    SDL_Surface* messageSurface = TTF_RenderText_Blended(largeFont, message.c_str(), whiteColor);
    if (messageSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);
    
    SDL_Rect messageRect = {
        (SCREEN_WIDTH - messageSurface->w) / 2,
        180,
        messageSurface->w,
        messageSurface->h
    };
    
    SDL_RenderCopy(renderer, messageTexture, NULL, &messageRect);
    SDL_FreeSurface(messageSurface);
    SDL_DestroyTexture(messageTexture);
    
    // Render player 1 score
    std::string p1ScoreStr = "Player 1 Score: " + std::to_string(score);
    SDL_Surface* p1ScoreSurface = TTF_RenderText_Blended(titleFont, p1ScoreStr.c_str(), whiteColor);
    if (p1ScoreSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p1ScoreTexture = SDL_CreateTextureFromSurface(renderer, p1ScoreSurface);
    
    SDL_Rect p1ScoreRect = {
        (SCREEN_WIDTH - p1ScoreSurface->w) / 2,
        260,
        p1ScoreSurface->w,
        p1ScoreSurface->h
    };
    
    SDL_RenderCopy(renderer, p1ScoreTexture, NULL, &p1ScoreRect);
    SDL_FreeSurface(p1ScoreSurface);
    SDL_DestroyTexture(p1ScoreTexture);
    
    // Render player 2 score
    std::string p2ScoreStr = "Player 2 Score: " + std::to_string(scoreP2);
    SDL_Surface* p2ScoreSurface = TTF_RenderText_Blended(titleFont, p2ScoreStr.c_str(), whiteColor);
    if (p2ScoreSurface == nullptr) {
        // Handle error
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_Texture* p2ScoreTexture = SDL_CreateTextureFromSurface(renderer, p2ScoreSurface);
    
    SDL_Rect p2ScoreRect = {
        (SCREEN_WIDTH - p2ScoreSurface->w) / 2,
        320,
        p2ScoreSurface->w,
        p2ScoreSurface->h
    };
    
    SDL_RenderCopy(renderer, p2ScoreTexture, NULL, &p2ScoreRect);
    SDL_FreeSurface(p2ScoreSurface);
    SDL_DestroyTexture(p2ScoreTexture);
    
    // Render buttons
    for (size_t i = 0; i < multiplayerGameOverButtons.size(); i++) {
        renderButton(multiplayerGameOverButtons[i], menuFont);
    }
    
    // Update screen
    SDL_RenderPresent(renderer);
}

void updateButtonHoverStates() {
    // Update menu buttons
    if (currentState == MENU) {
        for (size_t i = 0; i < menuButtons.size(); i++) {
            menuButtons[i].isHovered = isPointInRect(mouseX, mouseY, menuButtons[i].rect);
        }
    }
    // Update how to play buttons
    else if (currentState == HOW_TO_PLAY) {
        for (size_t i = 0; i < howToPlayButtons.size(); i++) {
            howToPlayButtons[i].isHovered = isPointInRect(mouseX, mouseY, howToPlayButtons[i].rect);
        }
    }
    // Update game over buttons
    else if (currentState == GAME_OVER) {
        for (size_t i = 0; i < gameOverButtons.size(); i++) {
            gameOverButtons[i].isHovered = isPointInRect(mouseX, mouseY, gameOverButtons[i].rect);
        }
    }
    // Update multiplayer game over buttons
    else if (currentState == MULTIPLAYER_GAME_OVER) {
        for (size_t i = 0; i < multiplayerGameOverButtons.size(); i++) {
            multiplayerGameOverButtons[i].isHovered = isPointInRect(mouseX, mouseY, multiplayerGameOverButtons[i].rect);
        }
    }
}

void restart() {
    // Reset boards
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = 0;
            boardP2[i][j] = 0;
        }
    }
    
    // Reset game state
    score = 0;
    scoreP2 = 0;
    gameOver = false;
    gameOverP2 = false;
    won = false;
    wonP2 = false;
    currentPlayer = PLAYER_ONE;
    
    // Clear animations
    animations.clear();
    animationsP2.clear();
    mergedTiles.clear();
    mergedTilesP2.clear();
    newTiles.clear();
    newTilesP2.clear();
    animating = false;
    
    // Mark the board texture as needing update
    boardTextureNeedsUpdate = true;
    
    // Add initial tiles to both boards in multiplayer mode
    if (currentState == MULTIPLAYER || currentState == MULTIPLAYER_GAME_OVER) {
        // Thêm trực tiếp vào bảng thay vì qua animation cho player 1
        std::vector<std::pair<int, int>> emptyP1Cells;
        
        // Tìm tất cả các ô trống cho player 1
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                emptyP1Cells.push_back(std::make_pair(i, j));
            }
        }
        
        // Chọn 2 ô ngẫu nhiên cho player 1
        std::shuffle(emptyP1Cells.begin(), emptyP1Cells.end(), rng);
        
        // Thêm ô đầu tiên cho player 1 (90% là 2, 10% là 4)
        std::uniform_int_distribution<int> valueDist(0, 9);
        int row1 = emptyP1Cells[0].first;
        int col1 = emptyP1Cells[0].second;
        board[row1][col1] = (valueDist(rng) < 9) ? 2 : 4;
        
        // Thêm ô thứ hai cho player 1 (90% là 2, 10% là 4)
        int row2 = emptyP1Cells[1].first;
        int col2 = emptyP1Cells[1].second;
        board[row2][col2] = (valueDist(rng) < 9) ? 2 : 4;
        
        // Thêm trực tiếp vào bảng thay vì qua animation cho player 2
        std::vector<std::pair<int, int>> emptyP2Cells;
        
        // Tìm tất cả các ô trống cho player 2
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                emptyP2Cells.push_back(std::make_pair(i, j));
            }
        }
        
        // Chọn 2 ô ngẫu nhiên cho player 2
        std::shuffle(emptyP2Cells.begin(), emptyP2Cells.end(), rng);
        
        // Thêm ô đầu tiên cho player 2 (90% là 2, 10% là 4)
        int row1P2 = emptyP2Cells[0].first;
        int col1P2 = emptyP2Cells[0].second;
        boardP2[row1P2][col1P2] = (valueDist(rng) < 9) ? 2 : 4;
        
        // Thêm ô thứ hai cho player 2 (90% là 2, 10% là 4)
        int row2P2 = emptyP2Cells[1].first;
        int col2P2 = emptyP2Cells[1].second;
        boardP2[row2P2][col2P2] = (valueDist(rng) < 9) ? 2 : 4;
        
        currentState = MULTIPLAYER;
    } else {
        // Single player mode - thêm 2 ô ngẫu nhiên
        // Thêm trực tiếp vào bảng thay vì qua animation
        std::vector<std::pair<int, int>> emptyCells;
        
        // Tìm tất cả các ô trống
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                emptyCells.push_back(std::make_pair(i, j));
            }
        }
        
        // Chọn 2 ô ngẫu nhiên
        std::shuffle(emptyCells.begin(), emptyCells.end(), rng);
        
        // Thêm ô đầu tiên (90% là 2, 10% là 4)
        std::uniform_int_distribution<int> valueDist(0, 9);
        int row1 = emptyCells[0].first;
        int col1 = emptyCells[0].second;
        board[row1][col1] = (valueDist(rng) < 9) ? 2 : 4;
        
        // Thêm ô thứ hai (90% là 2, 10% là 4)
        int row2 = emptyCells[1].first;
        int col2 = emptyCells[1].second;
        board[row2][col2] = (valueDist(rng) < 9) ? 2 : 4;
        
        currentState = PLAYING;
    }
    
    // Cập nhật bảng ngay lập tức
    boardTextureNeedsUpdate = true;
    updateBoardTexture();
    render();
    
    // Lưu game sau khi khởi tạo lại
    saveGame();
    
    // Play button sound
    playSound(buttonSound);
}

void updateAnimations() {
    // Calculate delta time
    auto currentTime = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    
    // Limit delta time to prevent large jumps that cause flickering
    dt = std::min(dt, 0.033f); // Cap at ~30 FPS equivalent
    
    lastFrameTime = currentTime;
    
    // Update total animation time
    deltaTime += dt;
    
    // Update animation progress
    std::vector<TileAnimation>& currentAnimations = (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) ? animationsP2 : animations;
    
    for (size_t i = 0; i < currentAnimations.size(); i++) {
        currentAnimations[i].progress += dt;
    }
    
    // Check if animations are complete
    bool allComplete = true;
    float totalAnimationTime = ANIMATION_DURATION + NEW_TILE_DELAY + NEW_TILE_ANIMATION_DURATION;
    
    if (deltaTime < totalAnimationTime) {
        allComplete = false;
    }
    
    if (allComplete) {
        // Reset animation state
        animating = false;
        deltaTime = 0.0f;
        
        // Clear animation data
        if (currentState == MULTIPLAYER && currentPlayer == PLAYER_TWO) {
            animationsP2.clear();
            mergedTilesP2.clear();
            newTilesP2.clear();
        } else {
            animations.clear();
            mergedTiles.clear();
            newTiles.clear();
        }
        
        // Mark the board texture as needing update
        boardTextureNeedsUpdate = true;
        
        // Force a final render to ensure the board is in its final state
        render();
        
        // Check game state after animations complete
        checkWin();
        checkGameOver();
    }
}

void handleMenuInput(SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
        updateButtonHoverStates();
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;
            
            // Check which button was clicked
            for (size_t i = 0; i < menuButtons.size(); i++) {
                if (isPointInRect(mouseX, mouseY, menuButtons[i].rect)) {
                    // Play button sound
                    playSound(buttonSound);
                    
                    // Handle button click
                    switch (i) {
                        case 0: // Singleplayer
                            // Tải game chế độ đơn nếu có
                            if (loadGame(false)) {
                                currentState = PLAYING;
                            } else {
                                // Không có game đã lưu, khởi tạo mới
                                currentState = PLAYING;
                                restart();
                            }
                            break;
                        case 1: // Multiplayer
                            // Tải game chế độ đa người nếu có
                            if (loadGame(true)) {
                                currentState = MULTIPLAYER;
                            } else {
                                // Không có game đã lưu, khởi tạo mới
                                currentState = MULTIPLAYER;
                                restart();
                            }
                            break;
                        case 2: // How to Play
                            currentState = HOW_TO_PLAY;
                            break;
                        case 3: // Exit
                            gameOver = true; // This will cause the application to exit
                            break;
                    }
                }
            }
        }
    }
}

void handleHowToPlayInput(SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
        updateButtonHoverStates();
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;
            
            // Check if back button was clicked
            if (isPointInRect(mouseX, mouseY, howToPlayButtons[0].rect)) {
                playSound(buttonSound);
                currentState = MENU;
            }
        }
    }
    else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) {
            playSound(buttonSound);
            currentState = MENU;
        }
    }
}

void handleGameOverInput(SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
        updateButtonHoverStates();
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;
            
            // Check which button was clicked
            for (size_t i = 0; i < gameOverButtons.size(); i++) {
                if (isPointInRect(mouseX, mouseY, gameOverButtons[i].rect)) {
                    playSound(buttonSound);
                    
                    // Handle button click
                    switch (i) {
                        case 0: // Play Again
                            restart();
                            break;
                        case 1: // Main Menu
                            currentState = MENU;
                            break;
                    }
                }
            }
        }
    }
    else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_r) {
            playSound(buttonSound);
            restart();
        }
        else if (e.key.keysym.sym == SDLK_ESCAPE) {
            playSound(buttonSound);
            currentState = MENU;
        }
    }
}

void handleMultiplayerGameOverInput(SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
        updateButtonHoverStates();
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;
            
            // Check which button was clicked
            for (size_t i = 0; i < multiplayerGameOverButtons.size(); i++) {
                if (isPointInRect(mouseX, mouseY, multiplayerGameOverButtons[i].rect)) {
                    playSound(buttonSound);
                    
                    // Handle button click
                    switch (i) {
                        case 0: // Play Again
                            restart();
                            break;
                        case 1: // Main Menu
                            currentState = MENU;
                            break;
                    }
                }
            }
        }
    }
    else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_r) {
            playSound(buttonSound);
            restart();
        }
        else if (e.key.keysym.sym == SDLK_ESCAPE) {
            playSound(buttonSound);
            currentState = MENU;
        }
    }
}

void handleGameInput(SDL_Event& e) {
    if (e.type == SDL_QUIT) {
        gameOver = true;
    }
    else if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;

            // Check if "Back" button was clicked
            Button backButton;
            backButton.rect.x = BOARD_MARGIN;
            backButton.rect.y = BOARD_MARGIN;
            backButton.rect.w = 100;
            backButton.rect.h = 40;

            if (isPointInRect(mouseX, mouseY, backButton.rect)) {
                playSound(buttonSound);
                saveGame(); // Lưu game khi quay lại menu
                currentState = MENU;
            }

            // Check if "New Game" button was clicked
            Button newGameButton;
            newGameButton.rect.x = BOARD_MARGIN + backButton.rect.w + 10;
            newGameButton.rect.y = BOARD_MARGIN;
            newGameButton.rect.w = 120;
            newGameButton.rect.h = 40;

            if (isPointInRect(mouseX, mouseY, newGameButton.rect)) {
                playSound(buttonSound);
                // Đảm bảo chúng ta ở chế độ chơi đơn
                currentState = PLAYING;
                restart();
            }
        }
    }
    else if (e.type == SDL_KEYDOWN && !animating) {
        bool moved = false;
        
        switch (e.key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_a:
                moved = moveLeft();
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                moved = moveRight();
                break;
            case SDLK_UP:
            case SDLK_w:
                moved = moveUp();
                break;
            case SDLK_DOWN:
            case SDLK_s:
                moved = moveDown();
                break;
            case SDLK_r:
                playSound(buttonSound);
                restart();
                break;
            case SDLK_ESCAPE:
                playSound(buttonSound);
                saveGame(); // Lưu game khi quay lại menu
                currentState = MENU;
                break;
        }
        
        if (moved) {
            // Start animation timer
            deltaTime = 0.0f;
            lastFrameTime = std::chrono::steady_clock::now();
            
            // Add a new tile after animation completes
            addRandomTile();
            checkWin();
            checkGameOver();
        }
    }
}

void handleMultiplayerInput(SDL_Event& e) {
    if (e.type == SDL_QUIT) {
        gameOver = true;
    }
    else if (e.type == SDL_MOUSEMOTION) {
        mouseX = e.motion.x;
        mouseY = e.motion.y;
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseX = e.button.x;
            mouseY = e.button.y;

            // Check if "Back" button was clicked
            Button backButton;
            backButton.rect.x = BOARD_MARGIN;
            backButton.rect.y = BOARD_MARGIN;
            backButton.rect.w = 100;
            backButton.rect.h = 40;

            if (isPointInRect(mouseX, mouseY, backButton.rect)) {
                playSound(buttonSound);
                saveGame(); // Lưu game khi quay lại menu
                currentState = MENU;
            }

            // Check if "New Game" button was clicked
            Button newGameButton;
            newGameButton.rect.x = BOARD_MARGIN + backButton.rect.w + 10;
            newGameButton.rect.y = BOARD_MARGIN;
            newGameButton.rect.w = 120;
            newGameButton.rect.h = 40;

            if (isPointInRect(mouseX, mouseY, newGameButton.rect)) {
                playSound(buttonSound);
                restart();
            }
        }
    }
    else if (e.type == SDL_KEYDOWN && !animating) {
        bool movedP1 = false;
        bool movedP2 = false;
        
        // Handle Player 1 controls (WASD)
        switch (e.key.keysym.sym) {
            case SDLK_a: // Left
                currentPlayer = PLAYER_ONE;
                movedP1 = moveLeft();
                break;
            case SDLK_d: // Right
                currentPlayer = PLAYER_ONE;
                movedP1 = moveRight();
                break;
            case SDLK_w: // Up
                currentPlayer = PLAYER_ONE;
                movedP1 = moveUp();
                break;
            case SDLK_s: // Down
                currentPlayer = PLAYER_ONE;
                movedP1 = moveDown();
                break;
        }
        
        // Handle Player 2 controls (Arrow keys)
        switch (e.key.keysym.sym) {
            case SDLK_LEFT:
                currentPlayer = PLAYER_TWO;
                movedP2 = moveLeft();
                break;
            case SDLK_RIGHT:
                currentPlayer = PLAYER_TWO;
                movedP2 = moveRight();
                break;
            case SDLK_UP:
                currentPlayer = PLAYER_TWO;
                movedP2 = moveUp();
                break;
            case SDLK_DOWN:
                currentPlayer = PLAYER_TWO;
                movedP2 = moveDown();
                break;
            case SDLK_r:
                playSound(buttonSound);
                restart();
                break;
            case SDLK_ESCAPE:
                playSound(buttonSound);
                saveGame(); // Lưu game khi quay lại menu
                currentState = MENU;
                break;
        }
        
        // Process Player 1's move
        if (movedP1) {
            // Start animation timer
            deltaTime = 0.0f;
            lastFrameTime = std::chrono::steady_clock::now();
            
            addRandomTile();
            checkWin();
            checkGameOver();
        }
        
        // Process Player 2's move
        if (movedP2) {
            // Start animation timer
            deltaTime = 0.0f;
            lastFrameTime = std::chrono::steady_clock::now();
            
            addRandomTile();
            checkWin();
            checkGameOver();
        }
    }
}

void render() {
    switch (currentState) {
        case MENU:
            renderMenu();
            break;
        case PLAYING:
            renderGameBoard();
            break;
        case MULTIPLAYER:
            renderMultiplayerGameBoard();
            break;
        case HOW_TO_PLAY:
            renderHowToPlay();
            break;
        case GAME_OVER:
            renderGameOver();
            break;
        case MULTIPLAYER_GAME_OVER:
            renderMultiplayerGameOver();
            break;
    }
}

void run() {
    // Main game loop
    bool quitApplication = false;
    Uint32 frameStart;
    const int FPS = 60;
    const int frameDelay = 1000 / FPS;
    
    while (!quitApplication) {
        frameStart = SDL_GetTicks();
        
        // Kiểm tra xem có cần lưu game tự động không
        if (SDL_GetTicks() - lastAutoSaveTime > AUTO_SAVE_INTERVAL && 
            (currentState == PLAYING || currentState == MULTIPLAYER)) {
            saveGame();
            lastAutoSaveTime = SDL_GetTicks();
        }
        
        // Process all pending events
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                saveGame(); // Lưu game khi thoát
                quitApplication = true;
                break;
            }
            
            // Handle input based on current state
            switch (currentState) {
                case MENU:
                    handleMenuInput(e);
                    break;
                case PLAYING:
                    handleGameInput(e);
                    break;
                case MULTIPLAYER:
                    handleMultiplayerInput(e);
                    break;
                case HOW_TO_PLAY:
                    handleHowToPlayInput(e);
                    break;
                case GAME_OVER:
                    handleGameOverInput(e);
                    break;
                case MULTIPLAYER_GAME_OVER:
                    handleMultiplayerGameOverInput(e);
                    break;
            }
        }
        
        // Update animations if needed
        if (animating) {
            updateAnimations();
        }
        
        // Check if we should exit the application
        if (gameOver && currentState == MENU) {
            saveGame(); // Lưu game khi thoát
            quitApplication = true;
        }
        
        // Render the current state
        render();
        
        // Cap frame rate for smoother animations
        int frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > frameTime) {
            SDL_Delay(frameDelay - frameTime);
        }
    }
}
};

int main(int argc, char* args[]) {
Game2048 game;

if (!game.initialize()) {
    std::cerr << "Failed to initialize game!" << std::endl;
    return 1;
}

game.run();

return 0;
}

