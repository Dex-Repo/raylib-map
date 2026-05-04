/*
 * Tilemap Editor v9 - With Open File functionality
 * COMPILE:
 *   macOS/Linux: gcc tilemap_editor_v9.c -o game $(pkg-config --libs --cflags raylib) && ./game
 *   If pkg-config fails: gcc tilemap_editor_v9.c -o game -lraylib && ./game
 * 
 * SHORTCUTS:
 *   Cmd/Ctrl+O - Open file dialog
 *   Cmd/Ctrl+S - Save file dialog
 *   S - Toggle slice panel
 *   H - Toggle help
 *   ESC - Close panels / Exit confirm
 */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TILES 1024
#define MAX_MAP_WIDTH 256
#define MAX_MAP_HEIGHT 256
#define MAX_TEXTURES 16
#define SIDEBAR_WIDTH 280
#define SIDEBAR_COLLAPSED_WIDTH 42
#define SIDEBAR_MIN_WIDTH 260
#define SIDEBAR_MAX_WIDTH 520
#define MAX_FILENAME 256
#define MAX_TILE_NAME 32
#define TILE_PREVIEW_SIZE 48
#define SCROLL_SPEED 30
#define MAX_MESSAGE_LEN 256

// Platform detection
#if defined(__APPLE__)
    #define KEY_MOD KEY_LEFT_SUPER
    #define MOD_SYMBOL "Cmd"
    #define MOD_TEXT "Cmd"
#else
    #define KEY_MOD KEY_LEFT_CONTROL
    #define MOD_SYMBOL "Ctrl"
    #define MOD_TEXT "Ctrl"
#endif

typedef struct {
    int id;
    int textureIndex;
    Rectangle sourceRect;
    char name[MAX_TILE_NAME];
} TileDef;

typedef struct {
    int tileId;
    uint8_t terrainType;
    int16_t ownerX;
    int16_t ownerY;
} MapCell;

typedef struct {
    MapCell cells[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    int width;
    int height;
    int defaultTileSize;
} TileMap;

typedef struct {
    Texture2D texture;
    char filename[MAX_FILENAME];
    int width, height;
    bool loaded;
} TextureAsset;

typedef enum {
    MODE_PAINT,
    MODE_ERASE,
    MODE_FILL
} EditorMode;

typedef enum {
    SLICE_GRID_AUTO,
    SLICE_GRID_MANUAL
} SliceMethod;

typedef enum {
    TERRAIN_WALKABLE = 0,
    TERRAIN_ICE = 1,
    TERRAIN_BLOCKED = 2,
    TERRAIN_TYPE_COUNT = 3
} TerrainType;

typedef enum {
    NUM_FIELD_NONE = 0,
    NUM_FIELD_MAP_W,
    NUM_FIELD_MAP_H,
    NUM_FIELD_SLICE_GRID_W,
    NUM_FIELD_SLICE_GRID_H,
    NUM_FIELD_SLICE_COLS,
    NUM_FIELD_SLICE_ROWS,
    NUM_FIELD_SLICE_OFFSET_X,
    NUM_FIELD_SLICE_OFFSET_Y,
    NUM_FIELD_SLICE_SPACING_X,
    NUM_FIELD_SLICE_SPACING_Y,
    NUM_FIELD_MANUAL_SNAP_X,
    NUM_FIELD_MANUAL_SNAP_Y
} NumericFieldId;

typedef struct {
    EditorMode mode;
    int selectedTileId;
    int hoveredTileId;
    int defaultTileSize;
    bool showGrid;
    bool showHelp;
    bool showSlicePanel;
    bool showSaveDialog;
    bool showOpenDialog;
    bool showExitConfirm;
    int sidebarScrollY;
    int maxSidebarScroll;
    
    // Messages
    char message[MAX_MESSAGE_LEN];
    float messageTimer;
    bool messageIsError;
    
    // Slice settings
    SliceMethod sliceMethod;
    int sliceTexIndex;
    int sliceGridW;
    int sliceGridH;
    int sliceCols;
    int sliceRows;
    int sliceOffsetX;
    int sliceOffsetY;
    int sliceSpacingX;
    int sliceSpacingY;
    int manualSnapX;
    int manualSnapY;
    
    // Preview
    bool showTilePreview;
    int previewTileId;

    // Save/export
    bool includeStandaloneMain;
    bool hasUnsavedChanges;
    int selectedTerrainType;
    bool terrainPaintOnly;
    float deleteConfirmTimer;
    bool requestExit;
    char openFilename[MAX_FILENAME];
    bool numericEditActive;
    int numericField;
    int numericMin;
    int numericMax;
    char numericBuffer[24];
    bool spinHoldActive;
    int spinHoldField;
    int spinHoldDelta;
    float spinHoldDelay;
    bool showSidebar;
    int sidebarWidth;
    bool resizingSidebar;
    int sliceTexturePage;
    bool sliceDraggingOffset;
    Vector2 sliceDragStartMouse;
    int sliceDragStartOffsetX;
    int sliceDragStartOffsetY;

    // Multi-selection for batch loading
    int selectedTileIndices[64];      // Max 64 selected tiles
    int selectedTileCount;
    int rangeStartIdx;                // For shift+click range selection
    int rangeEndIdx;
    bool rangeSelecting;

    // Import full image as map
    bool importFullImageMode;         // Toggle in Slice panel
    int importTileSize;               // Tile size for import (16/32/64/128)
    bool pixelPerfectImport;          // 1 pixel = 1 tile mode
} EditorState;

// Globals
TextureAsset g_textures[MAX_TEXTURES] = {0};
int g_textureCount = 0;

TileDef g_tileDefs[MAX_TILES] = {0};
int g_tileDefCount = 0;

TileMap g_map = {0};

EditorState g_state = {0};

Camera2D g_camera = {0};
Vector2 g_mouseWorld = {0};
bool g_draggingCamera = false;
Vector2 g_dragStart = {0};

char g_mapFilename[MAX_FILENAME] = "my_map";

// Colors (dark theme)
#define C_BG (Color){30, 30, 35, 255}
#define C_PANEL (Color){40, 40, 45, 255}
#define C_SIDEBAR (Color){25, 25, 30, 255}
#define C_GRID (Color){60, 60, 70, 255}
#define C_TEXT WHITE
#define C_TEXT_DIM (Color){150, 150, 150, 255}
#define C_ACCENT GREEN
#define C_ACCENT2 (Color){0, 200, 100, 255}
#define C_BUTTON (Color){50, 50, 60, 255}
#define C_BUTTON_HOVER (Color){70, 70, 80, 255}
#define C_MESSAGE_BG (Color){0, 100, 0, 200}
#define C_ERROR_BG (Color){100, 0, 0, 200}

// Prototypes
void InitEditor(void);
void UpdateEditor(void);
void DrawEditor(void);
void DrawSidebar(void);
void DrawHelp(void);
void DrawSlicePanel(void);
void DrawSaveDialog(void);
void DrawOpenDialog(void);
void DrawExitConfirm(void);
void DrawMessages(void);
void DrawTilePreviewPopup(void);
void HandleInput(void);

void ShowMessage(const char* msg, bool isError);
void MarkMapDirty(void);
void LoadTextureFile(const char* filename);
bool TileExists(int texIdx, Rectangle rect);
void GenerateTilesFromGrid(int texIdx, int tileW, int tileH, int cols, int rows, 
                           int offX, int offY, int spaceX, int spaceY);
void AddManualTile(int texIdx, Rectangle rect);

void PlaceTile(int mapX, int mapY);
void EraseTile(int mapX, int mapY);
void FillArea(int startX, int startY, int tileId);

bool SaveMapAsC(const char* filename, bool includeStandaloneMain);
bool LoadMapFromC(const char* filename);

Vector2 GetMouseMapPos(void);
bool IsMouseOverSidebar(void);
bool IsMouseOverSlicePanel(void);
bool IsMouseOverSaveDialog(void);
bool IsMouseOverOpenDialog(void);
bool IsMouseOverExitConfirm(void);
int GetSidebarTilesPerRow(void);
int GetSidebarSelectedSectionHeight(void);
int GetSidebarAllTilesStartY(void);
Rectangle GetSidebarTileSlotRect(int tileIndex);
int GetSidebarTileAtPosition(Vector2 point);
bool IsAnyModalOpen(void);
void ClampSidebarScroll(void);
bool ResizeMap(int newWidth, int newHeight);
const char* TerrainName(TerrainType t);
Color TerrainColor(TerrainType t);
bool ResolveSaveFilename(const char* input, char* output, size_t outputSize);
void WriteEscapedCString(FILE* f, const char* text);
bool GetTileFootprint(int tileId, int* outW, int* outH);
bool GetAnchorForCell(int x, int y, int* outAnchorX, int* outAnchorY);

// Multi-selection functions
void ToggleTileSelection(int tileIdx);
void SelectTileRange(int startIdx, int endIdx);
void ClearTileSelection(void);
bool IsTileSelected(int tileIdx);
void ImportImageAsMap(int texIdx, int tileSize);
void ClearTileAtAnchor(int anchorX, int anchorY, bool resetTerrain);
bool CanPlaceTileAt(int tileId, int anchorX, int anchorY);
void ApplyTileAt(int tileId, int anchorX, int anchorY);
void RebuildOwnershipFromAnchors(void);
void PaintTerrainForAnchor(int anchorX, int anchorY, uint8_t terrainType);
void SetNumericFieldValue(int fieldId, int value);
void BeginNumericEdit(int fieldId, int minValue, int maxValue, int currentValue);
void CommitNumericEdit(void);
void CancelNumericEdit(void);
void HandleNumericEditInput(void);
void DrawSpinControl(const char* label, int fieldId, int cx, int cy, int minValue, int maxValue);
int GetSidebarWidth(void);
void HandleSidebarResize(void);
void TrySetWindowIcon(void);

void TrySetWindowIcon(void)
{
    const char* candidates[] = {
        "logo.png",
        "./logo.png",
        "../Resources/logo.png",
        "../../Resources/logo.png"
    };
    int count = (int)(sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < count; i++) {
        if (!FileExists(candidates[i])) continue;
        Image icon = LoadImage(candidates[i]);
        if (icon.data != NULL) {
            SetWindowIcon(icon);
            UnloadImage(icon);
            return;
        }
    }
}

int main(void)
{
    // Enable window resize
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    
    InitWindow(1400, 900, "raylib-map (rMP) - Tilemap Editor");
    TrySetWindowIcon();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Disable ESC to close window
    
    InitEditor();
    
    // Auto-load textures
    FilePathList files = LoadDirectoryFiles(".");
    for (unsigned int i = 0; i < files.count && g_textureCount < MAX_TEXTURES; i++) {
        const char* ext = GetFileExtension(files.paths[i]);
        if (ext && (TextIsEqual(ext, ".png") || TextIsEqual(ext, ".jpg") || 
                   TextIsEqual(ext, ".jpeg") || TextIsEqual(ext, ".bmp"))) {
            LoadTextureFile(files.paths[i]);
        }
    }
    UnloadDirectoryFiles(files);
    
    while (true)
    {
        UpdateEditor();
        if (g_state.requestExit || WindowShouldClose()) break;
        
        BeginDrawing();
        ClearBackground(C_BG);
        
        DrawEditor();
        DrawSidebar();
        if (g_state.showSlicePanel) DrawSlicePanel();
        if (g_state.showSaveDialog) DrawSaveDialog();
        if (g_state.showOpenDialog) DrawOpenDialog();
        if (g_state.showHelp) DrawHelp();
        if (g_state.showExitConfirm) DrawExitConfirm();
        DrawTilePreviewPopup();
        DrawMessages();
        
        EndDrawing();
    }
    
    for (int i = 0; i < g_textureCount; i++) {
        if (g_textures[i].loaded) UnloadTexture(g_textures[i].texture);
    }
    
    CloseWindow();
    return 0;
}

void ShowMessage(const char* msg, bool isError)
{
    strncpy(g_state.message, msg, MAX_MESSAGE_LEN - 1);
    g_state.message[MAX_MESSAGE_LEN - 1] = '\0';
    int len = (int)strlen(g_state.message);
    g_state.messageTimer = 2.8f + (float)len * 0.015f;
    if (g_state.messageTimer > 7.0f) g_state.messageTimer = 7.0f;
    g_state.messageIsError = isError;
}

void MarkMapDirty(void)
{
    g_state.hasUnsavedChanges = true;
}

void InitEditor(void)
{
    g_camera.zoom = 1.0f;
    g_camera.offset = (Vector2){GetSidebarWidth() + 50, 50};
    g_camera.target = (Vector2){0, 0};
    
    g_map.width = 20;
    g_map.height = 15;
    g_map.defaultTileSize = 32;
    
    g_state.mode = MODE_PAINT;
    g_state.selectedTileId = -1;
    g_state.hoveredTileId = -1;
    g_state.defaultTileSize = 32;
    g_state.showGrid = true;
    g_state.showHelp = false;
    g_state.showSlicePanel = false;
    g_state.showSaveDialog = false;
    g_state.showOpenDialog = false;
    g_state.showExitConfirm = false;
    g_state.showTilePreview = false;
    g_state.previewTileId = -1;
    g_state.sidebarScrollY = 0;
    g_state.message[0] = '\0';
    g_state.messageTimer = 0;
    g_state.messageIsError = false;
    g_state.includeStandaloneMain = false;
    g_state.hasUnsavedChanges = false;
    g_state.selectedTerrainType = TERRAIN_WALKABLE;
    g_state.terrainPaintOnly = false;
    g_state.deleteConfirmTimer = 0.0f;
    g_state.requestExit = false;
    strncpy(g_state.openFilename, "my_map.c", MAX_FILENAME - 1);
    g_state.openFilename[MAX_FILENAME - 1] = '\0';
    g_state.numericEditActive = false;
    g_state.numericField = NUM_FIELD_NONE;
    g_state.numericMin = 0;
    g_state.numericMax = 0;
    g_state.numericBuffer[0] = '\0';
    g_state.spinHoldActive = false;
    g_state.spinHoldField = NUM_FIELD_NONE;
    g_state.spinHoldDelta = 0;
    g_state.spinHoldDelay = 0.0f;
    g_state.showSidebar = true;
    g_state.sidebarWidth = SIDEBAR_WIDTH;
    g_state.resizingSidebar = false;
    g_state.sliceTexturePage = 0;
    g_state.sliceDraggingOffset = false;
    g_state.sliceDragStartMouse = (Vector2){0};
    g_state.sliceDragStartOffsetX = 0;
    g_state.sliceDragStartOffsetY = 0;

    // Initialize multi-selection fields
    g_state.selectedTileCount = 0;
    g_state.rangeStartIdx = -1;
    g_state.rangeEndIdx = -1;
    g_state.rangeSelecting = false;
    for (int i = 0; i < 64; i++) g_state.selectedTileIndices[i] = -1;

    // Initialize import full image fields
    g_state.importFullImageMode = false;
    g_state.importTileSize = 32;
    g_state.pixelPerfectImport = false;

    g_state.sliceMethod = SLICE_GRID_AUTO;
    g_state.sliceTexIndex = 0;
    g_state.sliceGridW = 32;
    g_state.sliceGridH = 32;
    g_state.sliceCols = 4;
    g_state.sliceRows = 4;
    g_state.sliceOffsetX = 0;
    g_state.sliceOffsetY = 0;
    g_state.sliceSpacingX = 0;
    g_state.sliceSpacingY = 0;
    g_state.manualSnapX = 32;
    g_state.manualSnapY = 32;
    
    for (int y = 0; y < MAX_MAP_HEIGHT; y++) {
        for (int x = 0; x < MAX_MAP_WIDTH; x++) {
            g_map.cells[y][x].tileId = -1;
            g_map.cells[y][x].terrainType = TERRAIN_WALKABLE;
            g_map.cells[y][x].ownerX = -1;
            g_map.cells[y][x].ownerY = -1;
        }
    }
}

int GetSidebarWidth(void)
{
    return g_state.showSidebar ? g_state.sidebarWidth : SIDEBAR_COLLAPSED_WIDTH;
}

void HandleSidebarResize(void)
{
    int sw = GetSidebarWidth();
    Rectangle handle = {sw - 4, 0, 8, (float)GetScreenHeight()};

    if (g_state.showSidebar && !IsAnyModalOpen() && !g_state.numericEditActive &&
        CheckCollisionPointRec(GetMousePosition(), handle) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.resizingSidebar = true;
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_state.resizingSidebar = false;
    }

    if (g_state.resizingSidebar) {
        int nw = (int)GetMousePosition().x;
        if (nw < SIDEBAR_MIN_WIDTH) nw = SIDEBAR_MIN_WIDTH;
        if (nw > SIDEBAR_MAX_WIDTH) nw = SIDEBAR_MAX_WIDTH;
        g_state.sidebarWidth = nw;
        g_camera.offset.x = (float)(GetSidebarWidth() + 50);
    }
}

bool GetTileFootprint(int tileId, int* outW, int* outH)
{
    if (tileId < 0 || tileId >= g_tileDefCount || g_state.defaultTileSize <= 0) {
        if (outW) *outW = 1;
        if (outH) *outH = 1;
        return false;
    }

    TileDef* t = &g_tileDefs[tileId];
    int sw = (int)(t->sourceRect.width + 0.5f);
    int sh = (int)(t->sourceRect.height + 0.5f);
    if (sw < 1) sw = g_state.defaultTileSize;
    if (sh < 1) sh = g_state.defaultTileSize;

    int fw = (sw + g_state.defaultTileSize - 1) / g_state.defaultTileSize;
    int fh = (sh + g_state.defaultTileSize - 1) / g_state.defaultTileSize;
    if (fw < 1) fw = 1;
    if (fh < 1) fh = 1;

    if (outW) *outW = fw;
    if (outH) *outH = fh;
    return true;
}

bool GetAnchorForCell(int x, int y, int* outAnchorX, int* outAnchorY)
{
    if (x < 0 || x >= g_map.width || y < 0 || y >= g_map.height) return false;
    MapCell* c = &g_map.cells[y][x];
    if (c->ownerX < 0 || c->ownerY < 0) return false;
    if (outAnchorX) *outAnchorX = c->ownerX;
    if (outAnchorY) *outAnchorY = c->ownerY;
    return true;
}

void ClearTileAtAnchor(int anchorX, int anchorY, bool resetTerrain)
{
    if (anchorX < 0 || anchorX >= g_map.width || anchorY < 0 || anchorY >= g_map.height) return;

    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            if (g_map.cells[y][x].ownerX == anchorX && g_map.cells[y][x].ownerY == anchorY) {
                g_map.cells[y][x].tileId = -1;
                g_map.cells[y][x].ownerX = -1;
                g_map.cells[y][x].ownerY = -1;
                if (resetTerrain) {
                    g_map.cells[y][x].terrainType = TERRAIN_WALKABLE;
                }
            }
        }
    }
}

bool CanPlaceTileAt(int tileId, int anchorX, int anchorY)
{
    int fw = 1, fh = 1;
    if (!GetTileFootprint(tileId, &fw, &fh)) return false;

    if (anchorX < 0 || anchorY < 0 || anchorX + fw > g_map.width || anchorY + fh > g_map.height) {
        return false;
    }

    for (int y = anchorY; y < anchorY + fh; y++) {
        for (int x = anchorX; x < anchorX + fw; x++) {
            int ox = g_map.cells[y][x].ownerX;
            int oy = g_map.cells[y][x].ownerY;
            if (ox >= 0 && oy >= 0 && (ox != anchorX || oy != anchorY)) {
                return false;
            }
        }
    }

    return true;
}

void ApplyTileAt(int tileId, int anchorX, int anchorY)
{
    int fw = 1, fh = 1;
    if (!GetTileFootprint(tileId, &fw, &fh)) return;

    for (int y = anchorY; y < anchorY + fh; y++) {
        for (int x = anchorX; x < anchorX + fw; x++) {
            g_map.cells[y][x].ownerX = (int16_t)anchorX;
            g_map.cells[y][x].ownerY = (int16_t)anchorY;
            g_map.cells[y][x].tileId = -1;
        }
    }

    g_map.cells[anchorY][anchorX].tileId = tileId;
}

void RebuildOwnershipFromAnchors(void)
{
    static int anchors[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];

    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            anchors[y][x] = g_map.cells[y][x].tileId;
            g_map.cells[y][x].tileId = -1;
            g_map.cells[y][x].ownerX = -1;
            g_map.cells[y][x].ownerY = -1;
        }
    }

    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            int tileId = anchors[y][x];
            if (tileId < 0 || tileId >= g_tileDefCount) continue;
            if (CanPlaceTileAt(tileId, x, y)) {
                ApplyTileAt(tileId, x, y);
            }
        }
    }
}

void PaintTerrainForAnchor(int anchorX, int anchorY, uint8_t terrainType)
{
    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            if (g_map.cells[y][x].ownerX == anchorX && g_map.cells[y][x].ownerY == anchorY) {
                g_map.cells[y][x].terrainType = terrainType;
            }
        }
    }
}

void SetNumericFieldValue(int fieldId, int value)
{
    switch (fieldId) {
        case NUM_FIELD_MAP_W:
            ResizeMap(value, g_map.height);
            break;
        case NUM_FIELD_MAP_H:
            ResizeMap(g_map.width, value);
            break;
        case NUM_FIELD_SLICE_GRID_W:
            g_state.sliceGridW = value;
            break;
        case NUM_FIELD_SLICE_GRID_H:
            g_state.sliceGridH = value;
            break;
        case NUM_FIELD_SLICE_COLS:
            g_state.sliceCols = value;
            break;
        case NUM_FIELD_SLICE_ROWS:
            g_state.sliceRows = value;
            break;
        case NUM_FIELD_SLICE_OFFSET_X:
            g_state.sliceOffsetX = value;
            break;
        case NUM_FIELD_SLICE_OFFSET_Y:
            g_state.sliceOffsetY = value;
            break;
        case NUM_FIELD_SLICE_SPACING_X:
            g_state.sliceSpacingX = value;
            break;
        case NUM_FIELD_SLICE_SPACING_Y:
            g_state.sliceSpacingY = value;
            break;
        case NUM_FIELD_MANUAL_SNAP_X:
            g_state.manualSnapX = value;
            break;
        case NUM_FIELD_MANUAL_SNAP_Y:
            g_state.manualSnapY = value;
            break;
        default:
            break;
    }
}

void BeginNumericEdit(int fieldId, int minValue, int maxValue, int currentValue)
{
    g_state.numericEditActive = true;
    g_state.numericField = fieldId;
    g_state.numericMin = minValue;
    g_state.numericMax = maxValue;
    snprintf(g_state.numericBuffer, sizeof(g_state.numericBuffer), "%d", currentValue);
}

void CommitNumericEdit(void)
{
    if (!g_state.numericEditActive || g_state.numericField == NUM_FIELD_NONE) return;

    int value = atoi(g_state.numericBuffer);
    if (value < g_state.numericMin) value = g_state.numericMin;
    if (value > g_state.numericMax) value = g_state.numericMax;
    SetNumericFieldValue(g_state.numericField, value);

    g_state.numericEditActive = false;
    g_state.numericField = NUM_FIELD_NONE;
    g_state.numericBuffer[0] = '\0';
}

void CancelNumericEdit(void)
{
    g_state.numericEditActive = false;
    g_state.numericField = NUM_FIELD_NONE;
    g_state.numericBuffer[0] = '\0';
}

void HandleNumericEditInput(void)
{
    if (!g_state.numericEditActive) return;

    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(g_state.numericBuffer);
        if (len < (int)sizeof(g_state.numericBuffer) - 1 && key >= '0' && key <= '9') {
            g_state.numericBuffer[len] = (char)key;
            g_state.numericBuffer[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    static float backspaceDelay = 0.35f;
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(g_state.numericBuffer);
        if (len > 0) g_state.numericBuffer[len - 1] = '\0';
        backspaceDelay = 0.35f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        backspaceDelay -= GetFrameTime();
        if (backspaceDelay <= 0.0f) {
            int len = (int)strlen(g_state.numericBuffer);
            if (len > 0) g_state.numericBuffer[len - 1] = '\0';
            backspaceDelay = 0.05f;
        }
    } else {
        backspaceDelay = 0.35f;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        CommitNumericEdit();
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        CancelNumericEdit();
    }
}

void DrawSpinControl(const char* label, int fieldId, int cx, int cy, int minValue, int maxValue)
{
    Rectangle minusRect = (Rectangle){cx + 90, cy - 2, 30, 20};
    Rectangle valueRect = (Rectangle){cx + 125, cy - 2, 40, 20};
    Rectangle plusRect = (Rectangle){cx + 170, cy - 2, 30, 20};

    int currentValue = 0;
    switch (fieldId) {
        case NUM_FIELD_MAP_W: currentValue = g_map.width; break;
        case NUM_FIELD_MAP_H: currentValue = g_map.height; break;
        case NUM_FIELD_SLICE_GRID_W: currentValue = g_state.sliceGridW; break;
        case NUM_FIELD_SLICE_GRID_H: currentValue = g_state.sliceGridH; break;
        case NUM_FIELD_SLICE_COLS: currentValue = g_state.sliceCols; break;
        case NUM_FIELD_SLICE_ROWS: currentValue = g_state.sliceRows; break;
        case NUM_FIELD_SLICE_OFFSET_X: currentValue = g_state.sliceOffsetX; break;
        case NUM_FIELD_SLICE_OFFSET_Y: currentValue = g_state.sliceOffsetY; break;
        case NUM_FIELD_SLICE_SPACING_X: currentValue = g_state.sliceSpacingX; break;
        case NUM_FIELD_SLICE_SPACING_Y: currentValue = g_state.sliceSpacingY; break;
        case NUM_FIELD_MANUAL_SNAP_X: currentValue = g_state.manualSnapX; break;
        case NUM_FIELD_MANUAL_SNAP_Y: currentValue = g_state.manualSnapY; break;
        default: break;
    }

    DrawText(label, cx, cy, 10, C_TEXT_DIM);
    DrawRectangleRec(minusRect, C_BUTTON);
    DrawText("-", (int)minusRect.x + 10, cy, 12, C_TEXT);
    DrawRectangleRec(valueRect, C_BG);
    DrawRectangleLinesEx(valueRect, 1.0f, g_state.numericEditActive && g_state.numericField == fieldId ? C_ACCENT : C_GRID);
    DrawText((g_state.numericEditActive && g_state.numericField == fieldId) ? g_state.numericBuffer : TextFormat("%d", currentValue),
             (int)valueRect.x + 5, cy, 12, C_TEXT);
    DrawRectangleRec(plusRect, C_BUTTON);
    DrawText("+", (int)plusRect.x + 9, cy, 12, C_TEXT);

    if (CheckCollisionPointRec(GetMousePosition(), valueRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        BeginNumericEdit(fieldId, minValue, maxValue, currentValue);
    }

    int delta = 0;
    int holdField = NUM_FIELD_NONE;
    if (CheckCollisionPointRec(GetMousePosition(), minusRect)) {
        holdField = fieldId;
        delta = -1;
    } else if (CheckCollisionPointRec(GetMousePosition(), plusRect)) {
        holdField = fieldId;
        delta = 1;
    }

    if (holdField != NUM_FIELD_NONE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int nv = currentValue + delta;
        if (nv < minValue) nv = minValue;
        if (nv > maxValue) nv = maxValue;
        SetNumericFieldValue(fieldId, nv);
        g_state.spinHoldActive = true;
        g_state.spinHoldField = fieldId;
        g_state.spinHoldDelta = delta;
        g_state.spinHoldDelay = 0.35f;
        if (g_state.numericEditActive && g_state.numericField == fieldId) {
            CancelNumericEdit();
        }
    }

    if (g_state.spinHoldActive && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        g_state.spinHoldField == fieldId && g_state.spinHoldDelta == delta && holdField == fieldId) {
        g_state.spinHoldDelay -= GetFrameTime();
        if (g_state.spinHoldDelay <= 0.0f) {
            int nv = currentValue + delta;
            if (nv < minValue) nv = minValue;
            if (nv > maxValue) nv = maxValue;
            SetNumericFieldValue(fieldId, nv);
            g_state.spinHoldDelay = 0.05f;
        }
    }
}

void UpdateEditor(void)
{
    Vector2 mousePos = GetMousePosition();
    g_mouseWorld = GetScreenToWorld2D(mousePos, g_camera);
    HandleSidebarResize();
    
    // Update message timer
    if (g_state.messageTimer > 0) {
        g_state.messageTimer -= GetFrameTime();
        if (g_state.messageTimer < 0) g_state.messageTimer = 0;
    }

    if (g_state.deleteConfirmTimer > 0.0f) {
        g_state.deleteConfirmTimer -= GetFrameTime();
        if (g_state.deleteConfirmTimer < 0.0f) g_state.deleteConfirmTimer = 0.0f;
    }
    
    // Update hover preview
    if (IsMouseOverSidebar() && !g_state.showHelp && !g_state.showSlicePanel && 
           !g_state.showSaveDialog && !g_state.showOpenDialog && !g_state.showExitConfirm) {
        g_state.hoveredTileId = GetSidebarTileAtPosition(mousePos);
        g_state.showTilePreview = (g_state.hoveredTileId >= 0);
    } else {
        g_state.showTilePreview = false;
    }
    
    // Drag & drop
    if (IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        int loaded = 0;
        for (unsigned int i = 0; i < files.count; i++) {
            const char* ext = GetFileExtension(files.paths[i]);
            if (ext && (TextIsEqual(ext, ".png") || TextIsEqual(ext, ".jpg") || 
                       TextIsEqual(ext, ".jpeg") || TextIsEqual(ext, ".bmp"))) {
                int prevCount = g_textureCount;
                LoadTextureFile(files.paths[i]);
                if (g_textureCount > prevCount) loaded++;
            }
        }
        UnloadDroppedFiles(files);
        if (loaded > 0) {
            ShowMessage(TextFormat("Loaded %d texture(s)", loaded), false);
        }
    }
    
    HandleInput();
}

bool IsMouseOverSidebar(void) {
    Vector2 m = GetMousePosition();
    return m.x < GetSidebarWidth();
}

bool IsMouseOverSlicePanel(void) {
    if (!g_state.showSlicePanel) return false;
    Vector2 m = GetMousePosition();
    int pw = 900, ph = 650;
    int px = (GetScreenWidth() - pw) / 2;
    int py = (GetScreenHeight() - ph) / 2;
    return CheckCollisionPointRec(m, (Rectangle){px, py, pw, ph});
}

bool IsMouseOverSaveDialog(void) {
    if (!g_state.showSaveDialog) return false;
    Vector2 m = GetMousePosition();
    int w = 540, h = 280;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    return CheckCollisionPointRec(m, (Rectangle){x, y, w, h});
}

bool IsMouseOverExitConfirm(void) {
    if (!g_state.showExitConfirm) return false;
    Vector2 m = GetMousePosition();
    int w = 400, h = 150;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    return CheckCollisionPointRec(m, (Rectangle){x, y, w, h});
}

bool IsMouseOverOpenDialog(void) {
    if (!g_state.showOpenDialog) return false;
    Vector2 m = GetMousePosition();
    int w = 540, h = 280;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    return CheckCollisionPointRec(m, (Rectangle){x, y, w, h});
}

bool IsAnyModalOpen(void) {
    return g_state.showSaveDialog || g_state.showOpenDialog || g_state.showExitConfirm || g_state.showHelp || g_state.showSlicePanel;
}

void HandleInput(void)
{
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_state.spinHoldActive = false;
    }

    if (g_state.numericEditActive) {
        HandleNumericEditInput();
        return;
    }

    if (!IsAnyModalOpen() && IsKeyPressed(KEY_TAB)) {
        g_state.showSidebar = !g_state.showSidebar;
        g_camera.offset.x = (float)(GetSidebarWidth() + 50);
        return;
    }

    bool modPressed = IsKeyDown(KEY_MOD);

    if (modPressed && IsKeyPressed(KEY_S) && !IsAnyModalOpen()) {
        g_state.showSaveDialog = true;
        g_state.showSlicePanel = false;
        g_state.showHelp = false;
        return;
    }

    if (modPressed && IsKeyPressed(KEY_O) && !IsAnyModalOpen()) {
        g_state.showOpenDialog = true;
        g_state.showSlicePanel = false;
        g_state.showHelp = false;
        return;
    }

    if (!modPressed && IsKeyPressed(KEY_S) && !g_state.showHelp && !g_state.showSaveDialog && !g_state.showOpenDialog && !g_state.showExitConfirm) {
           g_state.showSlicePanel = !g_state.showSlicePanel;
           g_state.showOpenDialog = false;
        return;
    }

    if (IsKeyPressed(KEY_H) && !g_state.showSaveDialog && !g_state.showOpenDialog && !g_state.showExitConfirm) {
           g_state.showHelp = !g_state.showHelp;
           g_state.showOpenDialog = false;
        if (g_state.showHelp) g_state.showSlicePanel = false;
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (g_state.showExitConfirm) {
            g_state.showExitConfirm = false;
            return;
        }
        if (g_state.showHelp) {
            g_state.showHelp = false;
            return;
        }
        if (g_state.showSlicePanel) {
            g_state.showSlicePanel = false;
            return;
        }
        if (g_state.showSaveDialog) {
            g_state.showSaveDialog = false;
            return;
        }
        if (g_state.showOpenDialog) {
            g_state.showOpenDialog = false;
            return;
        }

        g_state.showExitConfirm = true;
        return;
    }

    if (g_state.showExitConfirm) {
        if (IsKeyPressed(KEY_Y)) {
            g_state.showExitConfirm = false;
            g_state.requestExit = true;
        } else if (IsKeyPressed(KEY_N)) {
            g_state.showExitConfirm = false;
        }
        return;
    }

    // Multi-selection keyboard shortcuts (when sidebar is active)
    if (IsMouseOverSidebar() && !IsAnyModalOpen()) {
        // Ctrl+A: Select all visible tiles
        if (modPressed && IsKeyPressed(KEY_A)) {
            ClearTileSelection();
            int perRow = GetSidebarTilesPerRow();
            int visibleRows = GetScreenHeight() / (TILE_PREVIEW_SIZE + 4);
            int startIdx = (g_state.sidebarScrollY / (TILE_PREVIEW_SIZE + 4)) * perRow;
            int endIdx = startIdx + (visibleRows * perRow);
            if (endIdx > g_tileDefCount) endIdx = g_tileDefCount;
            for (int i = startIdx; i < endIdx && g_state.selectedTileCount < 64; i++) {
                g_state.selectedTileIndices[g_state.selectedTileCount] = i;
                g_state.selectedTileCount++;
            }
            ShowMessage(TextFormat("Selected %d visible tiles", g_state.selectedTileCount), false);
            return;
        }

        // Enter: Set selected tiles as active for painting (use last selected as primary)
        if (IsKeyPressed(KEY_ENTER) && g_state.selectedTileCount > 0) {
            g_state.selectedTileId = g_state.selectedTileIndices[g_state.selectedTileCount - 1];
            g_state.mode = MODE_PAINT;
            ShowMessage(TextFormat("Active tile set to [%d] from selection", g_state.selectedTileId), false);
            return;
        }

        // Esc: Clear multi-selection
        if (IsKeyPressed(KEY_ESCAPE) && g_state.selectedTileCount > 0) {
            ClearTileSelection();
            ShowMessage("Selection cleared", false);
            return;
        }
    }

    if (IsAnyModalOpen()) {
        return;
    }

    if (IsKeyPressed(KEY_ONE)) g_state.mode = MODE_PAINT;
    if (IsKeyPressed(KEY_TWO)) g_state.mode = MODE_ERASE;
    if (IsKeyPressed(KEY_THREE)) g_state.mode = MODE_FILL;
    if (IsKeyPressed(KEY_SEVEN)) g_state.selectedTerrainType = TERRAIN_WALKABLE;
    if (IsKeyPressed(KEY_EIGHT)) g_state.selectedTerrainType = TERRAIN_ICE;
    if (IsKeyPressed(KEY_NINE)) g_state.selectedTerrainType = TERRAIN_BLOCKED;
    if (IsKeyPressed(KEY_T)) g_state.terrainPaintOnly = !g_state.terrainPaintOnly;

    // Grid toggle
    if (IsKeyPressed(KEY_G) && !g_state.showSlicePanel) {
        g_state.showGrid = !g_state.showGrid;
    }
    
    // SCROLL: Check focus
    bool overSidebar = IsMouseOverSidebar();
    bool overSlicePanel = IsMouseOverSlicePanel();
    bool overSaveDialog = IsMouseOverSaveDialog();
    bool overOpenDialog = IsMouseOverOpenDialog();
    bool overExitConfirm = IsMouseOverExitConfirm();
    bool overHelp = g_state.showHelp;
    bool overAnyPanel = overSlicePanel || overSaveDialog || overOpenDialog || overHelp || overExitConfirm;
    
    float wheel = GetMouseWheelMove();
    
    if (wheel != 0) {
        if (overSidebar && !overAnyPanel) {
            // Sidebar scroll
            g_state.sidebarScrollY -= (int)(wheel * SCROLL_SPEED);
            if (g_state.sidebarScrollY < 0) g_state.sidebarScrollY = 0;
            if (g_state.sidebarScrollY > g_state.maxSidebarScroll) 
                g_state.sidebarScrollY = g_state.maxSidebarScroll;
        }
        else if (!overAnyPanel) {
            // Canvas zoom
            Vector2 before = GetScreenToWorld2D(GetMousePosition(), g_camera);
            g_camera.zoom += wheel * 0.1f;
            if (g_camera.zoom < 0.1f) g_camera.zoom = 0.1f;
            if (g_camera.zoom > 5.0f) g_camera.zoom = 5.0f;
            Vector2 after = GetScreenToWorld2D(GetMousePosition(), g_camera);
            g_camera.target.x += before.x - after.x;
            g_camera.target.y += before.y - after.y;
        }
    }
    
    // Camera pan
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || 
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_ALT))) {
        g_draggingCamera = true;
        g_dragStart = GetMousePosition();
    }
    
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE) || 
        (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && g_draggingCamera)) {
        g_draggingCamera = false;
    }
    
    if (g_draggingCamera) {
        Vector2 m = GetMousePosition();
        g_camera.target.x += (g_dragStart.x - m.x) / g_camera.zoom;
        g_camera.target.y += (g_dragStart.y - m.y) / g_camera.zoom;
        g_dragStart = m;
    }
    
    // Map editing (only when not on UI)
    bool overUI = overSidebar || overAnyPanel;
    
    if (!overUI && !g_draggingCamera) {
        Vector2 mapPos = GetMouseMapPos();
        int mx = (int)mapPos.x;
        int my = (int)mapPos.y;
        
        if (mx >= 0 && mx < g_map.width && my >= 0 && my < g_map.height) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (g_state.mode == MODE_PAINT && g_state.selectedTileId >= 0) {
                    if (g_state.terrainPaintOnly) {
                        int ax = -1, ay = -1;
                        if (GetAnchorForCell(mx, my, &ax, &ay)) {
                            PaintTerrainForAnchor(ax, ay, (uint8_t)g_state.selectedTerrainType);
                            MarkMapDirty();
                        } else if (g_map.cells[my][mx].terrainType != g_state.selectedTerrainType) {
                            g_map.cells[my][mx].terrainType = (uint8_t)g_state.selectedTerrainType;
                            MarkMapDirty();
                        }
                    } else {
                        PlaceTile(mx, my);
                        int ax = -1, ay = -1;
                        if (GetAnchorForCell(mx, my, &ax, &ay)) {
                            PaintTerrainForAnchor(ax, ay, (uint8_t)g_state.selectedTerrainType);
                            MarkMapDirty();
                        }
                    }
                } else if (g_state.mode == MODE_ERASE) {
                    EraseTile(mx, my);
                }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                int ax = -1, ay = -1;
                if (GetAnchorForCell(mx, my, &ax, &ay)) {
                    PaintTerrainForAnchor(ax, ay, (uint8_t)g_state.selectedTerrainType);
                    MarkMapDirty();
                } else if (g_map.cells[my][mx].terrainType != g_state.selectedTerrainType) {
                    g_map.cells[my][mx].terrainType = (uint8_t)g_state.selectedTerrainType;
                    MarkMapDirty();
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && g_state.mode == MODE_FILL) {
                if (g_state.selectedTileId >= 0) FillArea(mx, my, g_state.selectedTileId);
            }
        }
    }
    
    // Delete tile
    if (IsKeyPressed(KEY_DELETE) && g_state.selectedTileId >= 0 && !g_state.showExitConfirm) {
        if (g_state.deleteConfirmTimer <= 0.0f) {
            g_state.deleteConfirmTimer = 2.0f;
            ShowMessage("Press Delete again to clear selected tile from map", false);
            return;
        }

        int removed = 0;
        for (int y = 0; y < g_map.height; y++) {
            for (int x = 0; x < g_map.width; x++) {
                if (g_map.cells[y][x].tileId == g_state.selectedTileId &&
                    g_map.cells[y][x].ownerX == x && g_map.cells[y][x].ownerY == y) {
                    int fw = 1, fh = 1;
                    GetTileFootprint(g_state.selectedTileId, &fw, &fh);
                    ClearTileAtAnchor(x, y, true);
                    removed += fw * fh;
                }
            }
        }
        g_state.deleteConfirmTimer = 0.0f;
        g_state.selectedTileId = -1;
        if (removed > 0) {
            MarkMapDirty();
            ShowMessage(TextFormat("Cleared %d tile(s) from map", removed), false);
        }
    }
}

void LoadTextureFile(const char* filename)
{
    const char* basename = GetFileName(filename);
    for (int i = 0; i < g_textureCount; i++) {
        const char* existing = GetFileName(g_textures[i].filename);
        if (TextIsEqual(existing, basename) || TextIsEqual(g_textures[i].filename, filename)) {
            ShowMessage(TextFormat("Texture already loaded: %s", basename), false);
            return;
        }
    }

    if (g_textureCount >= MAX_TEXTURES) {
        ShowMessage("Maximum textures reached!", true);
        return;
    }
    
    Texture2D tex = LoadTexture(filename);
    if (tex.id == 0) {
        ShowMessage(TextFormat("Failed to load: %s", GetFileName(filename)), true);
        return;
    }
    
    int idx = g_textureCount++;
    g_textures[idx].texture = tex;
    g_textures[idx].width = tex.width;
    g_textures[idx].height = tex.height;
    strncpy(g_textures[idx].filename, filename, MAX_FILENAME - 1);
    g_textures[idx].filename[MAX_FILENAME - 1] = '\0';
    g_textures[idx].loaded = true;
    
    // Auto-detect best grid size
    int sizes[] = {16, 32, 64, 128};
    int best = 32;
    for (int i = 0; i < 4; i++) {
        if (tex.width % sizes[i] == 0 && tex.height % sizes[i] == 0) {
            int c = tex.width / sizes[i];
            int r = tex.height / sizes[i];
            if (c >= 2 && c <= 20 && r >= 2 && r <= 20) {
                best = sizes[i];
                break;
            }
        }
    }
    
    g_state.sliceGridW = best;
    g_state.sliceGridH = best;
    g_state.sliceCols = tex.width / best;
    g_state.sliceRows = tex.height / best;
    if (g_state.sliceCols > 10) g_state.sliceCols = 10;
    if (g_state.sliceRows > 10) g_state.sliceRows = 10;
}

bool TileExists(int texIdx, Rectangle rect)
{
    if (texIdx < 0 || texIdx >= g_textureCount) return false;

    for (int i = 0; i < g_tileDefCount; i++) {
        TileDef* t = &g_tileDefs[i];
        if (t->textureIndex == texIdx &&
            (int)t->sourceRect.x == (int)rect.x &&
            (int)t->sourceRect.y == (int)rect.y &&
            (int)t->sourceRect.width == (int)rect.width &&
            (int)t->sourceRect.height == (int)rect.height) {
            return true;
        }
    }
    return false;
}

void GenerateTilesFromGrid(int texIdx, int tileW, int tileH, int cols, int rows,
                           int offX, int offY, int spaceX, int spaceY)
{
    if (texIdx < 0 || texIdx >= g_textureCount) return;
    if (tileW <= 0 || tileH <= 0 || cols <= 0 || rows <= 0 || offX < 0 || offY < 0 || spaceX < 0 || spaceY < 0) {
        ShowMessage("Invalid slice settings", true);
        return;
    }
    
    Texture2D tex = g_textures[texIdx].texture;
    int added = 0;
    int skipped = 0;
    
    for (int r = 0; r < rows && g_tileDefCount < MAX_TILES; r++) {
        for (int c = 0; c < cols && g_tileDefCount < MAX_TILES; c++) {
            float x = offX + c * (tileW + spaceX);
            float y = offY + r * (tileH + spaceY);
            
            if (x + tileW <= tex.width && y + tileH <= tex.height) {
                Rectangle rect = {x, y, tileW, tileH};
                if (TileExists(texIdx, rect)) {
                    skipped++;
                    continue;
                }
                
                TileDef* t = &g_tileDefs[g_tileDefCount];
                t->id = g_tileDefCount;
                t->textureIndex = texIdx;
                t->sourceRect = rect;
                snprintf(t->name, MAX_TILE_NAME, "%s_%d_%d",
                        GetFileNameWithoutExt(g_textures[texIdx].filename), r, c);
                g_tileDefCount++;
                added++;
            }
        }
    }

    if (g_tileDefCount >= MAX_TILES) {
        ShowMessage("Reached MAX_TILES limit", true);
    }
    
    if (added > 0) {
        MarkMapDirty();
        ShowMessage(TextFormat("Generated %d tile(s)", added), false);
    }
    if (skipped > 0) {
        ShowMessage(TextFormat("Skipped %d duplicate(s)", skipped), false);
    }
}

void AddManualTile(int texIdx, Rectangle rect)
{
    if (g_tileDefCount >= MAX_TILES || texIdx < 0 || texIdx >= g_textureCount) return;
    if (rect.width <= 0 || rect.height <= 0 || rect.x < 0 || rect.y < 0) {
        ShowMessage("Invalid manual tile rectangle", true);
        return;
    }
    if (rect.x + rect.width > g_textures[texIdx].width || rect.y + rect.height > g_textures[texIdx].height) {
        ShowMessage("Tile rectangle exceeds texture bounds", true);
        return;
    }
    
    if (TileExists(texIdx, rect)) {
        ShowMessage("Tile already exists!", true);
        return;
    }
    
    TileDef* t = &g_tileDefs[g_tileDefCount];
    t->id = g_tileDefCount;
    t->textureIndex = texIdx;
    t->sourceRect = rect;
    snprintf(t->name, MAX_TILE_NAME, "%s_manual_%d",
            GetFileNameWithoutExt(g_textures[texIdx].filename), g_tileDefCount);
    g_tileDefCount++;
    MarkMapDirty();
    
    ShowMessage(TextFormat("Added tile [%d] %s", t->id, t->name), false);
}

Vector2 GetMouseMapPos(void)
{
    if (g_state.defaultTileSize <= 0) {
        return (Vector2){-1, -1};
    }

    return (Vector2){
        g_mouseWorld.x / g_state.defaultTileSize,
        g_mouseWorld.y / g_state.defaultTileSize
    };
}

void PlaceTile(int mx, int my)
{
    if (mx < 0 || mx >= g_map.width || my < 0 || my >= g_map.height) return;
    if (g_state.selectedTileId < 0 || g_state.selectedTileId >= g_tileDefCount) return;

    int anchorX = mx;
    int anchorY = my;
    if (GetAnchorForCell(mx, my, &anchorX, &anchorY)) {
        if (anchorX == mx && anchorY == my && g_map.cells[my][mx].tileId == g_state.selectedTileId) return;
        if (anchorX != mx || anchorY != my) {
            ShowMessage("Cell already occupied by a larger tile", true);
            return;
        }
        ClearTileAtAnchor(anchorX, anchorY, false);
    }

    if (!CanPlaceTileAt(g_state.selectedTileId, mx, my)) {
        ShowMessage("Tile footprint does not fit or overlaps another tile", true);
        return;
    }

    ApplyTileAt(g_state.selectedTileId, mx, my);
    MarkMapDirty();
}

void EraseTile(int mx, int my)
{
    if (mx < 0 || mx >= g_map.width || my < 0 || my >= g_map.height) return;
    int anchorX = -1, anchorY = -1;
    if (!GetAnchorForCell(mx, my, &anchorX, &anchorY)) return;
    ClearTileAtAnchor(anchorX, anchorY, true);
    MarkMapDirty();
}

void FillArea(int sx, int sy, int tileId)
{
    if (sx < 0 || sx >= g_map.width || sy < 0 || sy >= g_map.height) return;
    if (tileId < 0 || tileId >= g_tileDefCount) return;

    int targetAnchorX = sx;
    int targetAnchorY = sy;
    if (!GetAnchorForCell(sx, sy, &targetAnchorX, &targetAnchorY)) return;
    if (g_map.cells[targetAnchorY][targetAnchorX].tileId < 0) return;

    int target = g_map.cells[targetAnchorY][targetAnchorX].tileId;
    if (target == tileId) return;

    int cellCount = g_map.width * g_map.height;
    int* qx = (int*)malloc(sizeof(int) * cellCount);
    int* qy = (int*)malloc(sizeof(int) * cellCount);
    unsigned char* vis = (unsigned char*)calloc((size_t)cellCount, sizeof(unsigned char));
    if (!qx || !qy || !vis) {
        free(qx);
        free(qy);
        free(vis);
        ShowMessage("Fill failed: out of memory", true);
        return;
    }

    int qs = 0, qe = 0;
    int filled = 0;
    
    qx[qe] = targetAnchorX; qy[qe] = targetAnchorY; qe++;
    vis[targetAnchorY * g_map.width + targetAnchorX] = 1;
    
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = {0, 0, -1, 1};
    
    while (qs < qe) {
        int cx = qx[qs], cy = qy[qs]; qs++;
        if (g_map.cells[cy][cx].tileId < 0 || g_map.cells[cy][cx].ownerX != cx || g_map.cells[cy][cx].ownerY != cy) {
            continue;
        }
        ClearTileAtAnchor(cx, cy, false);
        if (!CanPlaceTileAt(tileId, cx, cy)) continue;
        ApplyTileAt(tileId, cx, cy);
        PaintTerrainForAnchor(cx, cy, (uint8_t)g_state.selectedTerrainType);
        filled++;
        
        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if (nx >= 0 && nx < g_map.width && ny >= 0 && ny < g_map.height) {
                int ax = -1, ay = -1;
                if (!GetAnchorForCell(nx, ny, &ax, &ay)) continue;
                if (ax < 0 || ay < 0 || ax >= g_map.width || ay >= g_map.height) continue;
                if (vis[ay * g_map.width + ax]) continue;
                if (g_map.cells[ay][ax].tileId != target) continue;
                vis[ay * g_map.width + ax] = 1;
                qx[qe] = ax; qy[qe] = ay; qe++;
            }
        }
    }

    free(qx);
    free(qy);
    free(vis);
    MarkMapDirty();
    ShowMessage(TextFormat("Filled %d cell(s)", filled), false);
}

int GetSidebarTilesPerRow(void)
{
    int perRow = (GetSidebarWidth() - 20) / (TILE_PREVIEW_SIZE + 4);
    return (perRow < 1) ? 1 : perRow;
}

int GetSidebarSelectedSectionHeight(void)
{
    if (g_state.selectedTileId < 0 || g_state.selectedTileId >= g_tileDefCount) {
        return 25;
    }

    TileDef* t = &g_tileDefs[g_state.selectedTileId];
    int height = 30;
    if (t->textureIndex >= 0 && t->textureIndex < g_textureCount &&
        g_textures[t->textureIndex].loaded && t->sourceRect.width > 0 && t->sourceRect.height > 0) {
        float scale = 1.0f;
        if (t->sourceRect.width > 80) scale = 80.0f / t->sourceRect.width;
        height += (int)(t->sourceRect.height * scale) + 10;
    }

    return height;
}

int GetSidebarAllTilesStartY(void)
{
    int y = 10 - g_state.sidebarScrollY;

    y += 25;
    y += 20;
    y += 25;

    y += 10;
    y += 20;
    if (g_textureCount == 0) {
        y += 20;
    } else {
        y += g_textureCount * 27;
    }
    y += 10;

    y += 10;
    y += 20;
    y += 3 * 30;
    y += 35;

    // Map size section
    y += 10;
    y += 20;
    y += 58;

    // Terrain section
    y += 10;
    y += 20;
    y += 4 * 30;
    y += 8;

    y += 10;
    y += 20;
    y += GetSidebarSelectedSectionHeight();
    y += 15;

    y += 10;
    y += 15;
    y += 20;

    return y;
}

Rectangle GetSidebarTileSlotRect(int tileIndex)
{
    if (tileIndex < 0 || tileIndex >= g_tileDefCount) {
        return (Rectangle){0, 0, 0, 0};
    }

    int perRow = GetSidebarTilesPerRow();
    int row = tileIndex / perRow;
    int col = tileIndex % perRow;
    int startY = GetSidebarAllTilesStartY();

    return (Rectangle){
        10 + col * (TILE_PREVIEW_SIZE + 4),
        startY + row * (TILE_PREVIEW_SIZE + 4),
        TILE_PREVIEW_SIZE,
        TILE_PREVIEW_SIZE
    };
}

int GetSidebarTileAtPosition(Vector2 point)
{
    if (!IsMouseOverSidebar()) return -1;

    for (int i = 0; i < g_tileDefCount; i++) {
        Rectangle slot = GetSidebarTileSlotRect(i);
        if (slot.y + slot.height < 0 || slot.y > GetScreenHeight()) continue;
        if (CheckCollisionPointRec(point, slot)) {
            return i;
        }
    }

    return -1;
}

void ClampSidebarScroll(void)
{
    if (g_state.sidebarScrollY < 0) g_state.sidebarScrollY = 0;
    if (g_state.sidebarScrollY > g_state.maxSidebarScroll) g_state.sidebarScrollY = g_state.maxSidebarScroll;
}

// Multi-selection functions implementation
void ToggleTileSelection(int tileIdx)
{
    if (tileIdx < 0 || tileIdx >= g_tileDefCount) return;

    // Check if already selected
    for (int i = 0; i < g_state.selectedTileCount; i++) {
        if (g_state.selectedTileIndices[i] == tileIdx) {
            // Remove from selection
            for (int j = i; j < g_state.selectedTileCount - 1; j++) {
                g_state.selectedTileIndices[j] = g_state.selectedTileIndices[j + 1];
            }
            g_state.selectedTileCount--;
            ShowMessage(TextFormat("Deselected tile [%d]", tileIdx), false);
            return;
        }
    }

    // Add to selection
    if (g_state.selectedTileCount < 64) {
        g_state.selectedTileIndices[g_state.selectedTileCount] = tileIdx;
        g_state.selectedTileCount++;
        ShowMessage(TextFormat("Selected %d tile(s)", g_state.selectedTileCount), false);
    }
}

bool IsTileSelected(int tileIdx)
{
    for (int i = 0; i < g_state.selectedTileCount; i++) {
        if (g_state.selectedTileIndices[i] == tileIdx) return true;
    }
    return false;
}

void SelectTileRange(int startIdx, int endIdx)
{
    // Clear existing selection
    ClearTileSelection();

    // Ensure valid range
    if (startIdx < 0) startIdx = 0;
    if (endIdx >= g_tileDefCount) endIdx = g_tileDefCount - 1;
    if (startIdx > endIdx) { int tmp = startIdx; startIdx = endIdx; endIdx = tmp; }

    // Select all tiles in range
    for (int i = startIdx; i <= endIdx && g_state.selectedTileCount < 64; i++) {
        g_state.selectedTileIndices[g_state.selectedTileCount] = i;
        g_state.selectedTileCount++;
    }

    ShowMessage(TextFormat("Selected range [%d] to [%d] (%d tiles)", startIdx, endIdx, g_state.selectedTileCount), false);
}

void ClearTileSelection(void)
{
    g_state.selectedTileCount = 0;
    for (int i = 0; i < 64; i++) g_state.selectedTileIndices[i] = -1;
    g_state.rangeStartIdx = -1;
    g_state.rangeEndIdx = -1;
    g_state.rangeSelecting = false;
}

bool ResizeMap(int newWidth, int newHeight)
{
    if (newWidth < 1 || newHeight < 1 || newWidth > MAX_MAP_WIDTH || newHeight > MAX_MAP_HEIGHT) {
        ShowMessage("Invalid map size", true);
        return false;
    }

    if (newWidth == g_map.width && newHeight == g_map.height) return true;

    MapCell backup[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    int oldW = g_map.width;
    int oldH = g_map.height;

    for (int y = 0; y < MAX_MAP_HEIGHT; y++) {
        for (int x = 0; x < MAX_MAP_WIDTH; x++) {
            backup[y][x] = g_map.cells[y][x];
            g_map.cells[y][x].tileId = -1;
            g_map.cells[y][x].terrainType = TERRAIN_WALKABLE;
            g_map.cells[y][x].ownerX = -1;
            g_map.cells[y][x].ownerY = -1;
        }
    }

    int copyW = (newWidth < oldW) ? newWidth : oldW;
    int copyH = (newHeight < oldH) ? newHeight : oldH;
    for (int y = 0; y < copyH; y++) {
        for (int x = 0; x < copyW; x++) {
            g_map.cells[y][x] = backup[y][x];
        }
    }

    g_map.width = newWidth;
    g_map.height = newHeight;
    RebuildOwnershipFromAnchors();
    MarkMapDirty();
    ShowMessage(TextFormat("Resized map to %dx%d", g_map.width, g_map.height), false);
    return true;
}

const char* TerrainName(TerrainType t)
{
    switch (t) {
        case TERRAIN_WALKABLE: return "Walkable";
        case TERRAIN_ICE: return "Ice";
        case TERRAIN_BLOCKED: return "Blocked";
        default: return "Unknown";
    }
}

Color TerrainColor(TerrainType t)
{
    switch (t) {
        case TERRAIN_ICE: return SKYBLUE;
        case TERRAIN_BLOCKED: return RED;
        case TERRAIN_WALKABLE:
        default: return GREEN;
    }
}

bool ResolveSaveFilename(const char* input, char* output, size_t outputSize)
{
    if (!input || !output || outputSize < 3) return false;

    while (*input == ' ' || *input == '\t') {
        input++;
    }

    size_t len = strlen(input);
    while (len > 0 && (input[len - 1] == ' ' || input[len - 1] == '\t')) {
        len--;
    }

    if (len == 0 || len >= outputSize) return false;

    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)input[i];
        if (ch < 32 || strchr("/\\:*?\"<>|", ch) != NULL) {
            return false;
        }
    }

    if (len == 1 && input[0] == '.') return false;
    if (len == 2 && input[0] == '.' && input[1] == '.') return false;

    bool hasCExt = len >= 2 && input[len - 2] == '.' && (input[len - 1] == 'c' || input[len - 1] == 'C');
    size_t finalLen = len + (hasCExt ? 0 : 2);
    if (finalLen >= outputSize) return false;

    memcpy(output, input, len);
    output[len] = '\0';

    if (!hasCExt) {
        output[len] = '.';
        output[len + 1] = 'c';
        output[len + 2] = '\0';
    }

    return true;
}

void WriteEscapedCString(FILE* f, const char* text)
{
    fputc('"', f);
    if (text) {
        for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
            if (*p == '\\' || *p == '"') {
                fputc('\\', f);
                fputc(*p, f);
            } else if (*p == '\n') {
                fputs("\\n", f);
            } else if (*p == '\r') {
                fputs("\\r", f);
            } else if (*p == '\t') {
                fputs("\\t", f);
            } else {
                fputc(*p, f);
            }
        }
    }
    fputc('"', f);
}

void DrawEditor(void)
{
    int sw = GetSidebarWidth();
    DrawRectangle(sw, 0, GetScreenWidth() - sw, GetScreenHeight(), C_BG);
    
    BeginMode2D(g_camera);
    
    // Grid
    if (g_state.showGrid) {
        for (int x = 0; x <= g_map.width; x++) {
            DrawLine(x * g_state.defaultTileSize, 0,
                    x * g_state.defaultTileSize, g_map.height * g_state.defaultTileSize,
                    C_GRID);
        }
        for (int y = 0; y <= g_map.height; y++) {
            DrawLine(0, y * g_state.defaultTileSize,
                    g_map.width * g_state.defaultTileSize, y * g_state.defaultTileSize,
                    C_GRID);
        }
    }
    
    // Map bounds
    DrawRectangleLines(0, 0,
                      g_map.width * g_state.defaultTileSize,
                      g_map.height * g_state.defaultTileSize, C_TEXT_DIM);
    
    // Tiles
    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            int tid = g_map.cells[y][x].tileId;
            if (tid >= 0 && tid < g_tileDefCount &&
                g_map.cells[y][x].ownerX == x && g_map.cells[y][x].ownerY == y) {
                TileDef* t = &g_tileDefs[tid];
                if (t->textureIndex >= 0 && t->textureIndex < g_textureCount &&
                    g_textures[t->textureIndex].loaded) {
                    DrawTextureRec(g_textures[t->textureIndex].texture, t->sourceRect,
                                 (Vector2){x * g_state.defaultTileSize,
                                          y * g_state.defaultTileSize}, WHITE);
                }
            }
        }
    }
    
    // Terrain hints overlay
    for (int y = 0; y < g_map.height; y++) {
        for (int x = 0; x < g_map.width; x++) {
            uint8_t terrain = g_map.cells[y][x].terrainType;
            if (terrain == TERRAIN_WALKABLE) continue;
            Color tc = ColorAlpha(TerrainColor((TerrainType)terrain), g_state.terrainPaintOnly ? 0.50f : 0.25f);
            DrawRectangle(x * g_state.defaultTileSize + 2, y * g_state.defaultTileSize + 2, 8, 8, tc);
        }
    }

    // Hover
    bool overUI = IsMouseOverSidebar() || g_state.showHelp || g_state.showSlicePanel || 
                      g_state.showSaveDialog || g_state.showOpenDialog || g_state.showExitConfirm;
    
    if (!overUI && !g_draggingCamera) {
        Vector2 mp = GetMouseMapPos();
        int mx = (int)mp.x, my = (int)mp.y;
        if (mx >= 0 && mx < g_map.width && my >= 0 && my < g_map.height) {
            Rectangle r = {mx * g_state.defaultTileSize, my * g_state.defaultTileSize,
                          g_state.defaultTileSize, g_state.defaultTileSize};
            Color c = (g_state.mode == MODE_ERASE) ? RED : C_ACCENT;
            DrawRectangleRec(r, ColorAlpha(c, 0.3f));
            DrawRectangleLinesEx(r, 2, c);
        }
    }
    
    EndMode2D();
    
    // Top bar
    DrawRectangle(sw, 0, GetScreenWidth() - sw, 34, C_PANEL);
    const char* modes[] = {"PAINT", "ERASE", "FILL"};
    char info[256];
    snprintf(info, sizeof(info), "[1/2/3] %s | Zoom: %.1fx | Map: %dx%d | Tiles: %d | Terrain:%s%s | %s%s+S Save | S Slice | H Help | TAB Sidebar | ESC Exit",
             modes[g_state.mode], g_camera.zoom, g_map.width, g_map.height, g_tileDefCount,
             TerrainName((TerrainType)g_state.selectedTerrainType), g_state.terrainPaintOnly ? "(paint-only)" : "",
             g_state.hasUnsavedChanges ? "* " : "", MOD_SYMBOL);

    Rectangle bSave = {sw + 8, 4, 84, 26};
    Rectangle bOpen = {sw + 96, 4, 84, 26};
    Rectangle bSlice = {sw + 184, 4, 84, 26};
    Rectangle bHelp = {sw + 272, 4, 84, 26};
    Rectangle bGrid = {sw + 360, 4, 84, 26};

    DrawRectangleRec(bSave, C_BUTTON); DrawText("Save", (int)bSave.x + 10, 11, 10, C_TEXT);
    DrawRectangleRec(bOpen, C_BUTTON); DrawText("Open", (int)bOpen.x + 10, 11, 10, C_TEXT);
    DrawRectangleRec(bSlice, g_state.showSlicePanel ? C_ACCENT : C_BUTTON); DrawText("Slice", (int)bSlice.x + 10, 11, 10, g_state.showSlicePanel ? BLACK : C_TEXT);
    DrawRectangleRec(bHelp, g_state.showHelp ? C_ACCENT : C_BUTTON); DrawText("Help", (int)bHelp.x + 10, 11, 10, g_state.showHelp ? BLACK : C_TEXT);
    DrawRectangleRec(bGrid, g_state.showGrid ? C_ACCENT : C_BUTTON); DrawText("Grid", (int)bGrid.x + 10, 11, 10, g_state.showGrid ? BLACK : C_TEXT);

    Vector2 mouse = GetMousePosition();
    if (!IsAnyModalOpen() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, bSave)) g_state.showSaveDialog = true;
        if (CheckCollisionPointRec(mouse, bOpen)) g_state.showOpenDialog = true;
        if (CheckCollisionPointRec(mouse, bSlice)) g_state.showSlicePanel = !g_state.showSlicePanel;
        if (CheckCollisionPointRec(mouse, bHelp)) g_state.showHelp = !g_state.showHelp;
        if (CheckCollisionPointRec(mouse, bGrid)) g_state.showGrid = !g_state.showGrid;
    }

    DrawText(info, sw + 452, 12, 10, C_TEXT);
}

void DrawSidebar(void)
{
    int sw = GetSidebarWidth();
    DrawRectangle(0, 0, sw, GetScreenHeight(), C_SIDEBAR);
    DrawLine(sw, 0, sw, GetScreenHeight(), C_GRID);

    Rectangle toggleRect = {4, 6, 34, 28};
    DrawRectangleRec(toggleRect, C_BUTTON);
    DrawText(g_state.showSidebar ? "<" : ">", 15, 14, 14, C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), toggleRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !IsAnyModalOpen()) {
        g_state.showSidebar = !g_state.showSidebar;
        g_camera.offset.x = (float)(GetSidebarWidth() + 50);
    }

    if (!g_state.showSidebar) {
        DrawText("UI", 12, 42, 10, C_TEXT_DIM);
        DrawText("TAB", 9, 56, 10, C_TEXT_DIM);
        return;
    }
    
    BeginScissorMode(0, 0, sw, GetScreenHeight());
    
    int y = 10 - g_state.sidebarScrollY;
    
    // Header
    DrawText("TILEMAP EDITOR", 10, y, 16, C_ACCENT);
    y += 25;
    DrawText("v1 - Modular Map Export", 10, y, 10, C_TEXT_DIM);
    y += 20;
    
    // Shortcuts with Mac symbol
    char shortcuts[128];
    snprintf(shortcuts, sizeof(shortcuts), "%s+S:Save S:Slice H:Help ESC:Exit", MOD_SYMBOL);
    DrawText(shortcuts, 10, y, 9, C_TEXT_DIM);
    y += 25;
    
    // Textures
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText(TextFormat("TEXTURES (%d):", g_textureCount), 10, y, 11, C_ACCENT2);
    y += 20;
    
    for (int i = 0; i < g_textureCount; i++) {
        DrawText(TextFormat("[%d] %s", i, GetFileName(g_textures[i].filename)),
                10, y, 9, C_TEXT);
        y += 12;
        DrawText(TextFormat("    %dx%d", g_textures[i].width, g_textures[i].height),
                10, y, 8, C_TEXT_DIM);
        y += 15;
    }
    
    if (g_textureCount == 0) {
        DrawText("Drop PNG/JPG files", 10, y, 10, C_TEXT_DIM);
        y += 20;
    }
    y += 10;
    
    // Tools
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText("TOOLS:", 10, y, 11, C_ACCENT2);
    y += 20;
    
    const char* toolNames[] = {"[1] PAINT", "[2] ERASE", "[3] FILL"};
    for (int i = 0; i < 3; i++) {
        Color c = ((int)g_state.mode == i) ? C_ACCENT : C_BUTTON;
        DrawRectangle(10, y, 110, 24, c);
        DrawText(toolNames[i], 18, y + 6, 10, ((int)g_state.mode == i) ? BLACK : C_TEXT);
        if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){10, y, 110, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_state.mode = (EditorMode)i;
        }
        y += 30;
    }
    
    DrawRectangle(10, y, 110, 24, g_state.showSlicePanel ? C_ACCENT : C_BUTTON);
    DrawText("[S] SLICE", 18, y + 6, 10, g_state.showSlicePanel ? BLACK : C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){10, y, 110, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showSlicePanel = !g_state.showSlicePanel;
    }

    DrawRectangle(126, y, 70, 24, C_BUTTON);
    DrawText("Save", 132, y + 6, 10, C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){126, y, 70, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showSaveDialog = true;
    }

    DrawRectangle(200, y, 70, 24, C_BUTTON);
    DrawText("Open", 206, y + 6, 10, C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){200, y, 70, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showOpenDialog = true;
    }
    y += 35;

    // Map size
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText("MAP SIZE:", 10, y, 11, C_ACCENT2);
    y += 20;
    DrawSpinControl("Width", NUM_FIELD_MAP_W, 10, y, 1, MAX_MAP_WIDTH);
    y += 28;
    DrawSpinControl("Height", NUM_FIELD_MAP_H, 10, y, 1, MAX_MAP_HEIGHT);
    y += 30;
    DrawText("Canvas resize keeps existing cells", 10, y, 9, C_TEXT_DIM);
    y += 28;

    // Terrain controls
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText("TERRAIN TYPE:", 10, y, 11, C_ACCENT2);
    y += 20;
    const TerrainType terrainOrder[3] = {TERRAIN_WALKABLE, TERRAIN_ICE, TERRAIN_BLOCKED};
    const char* terrainKeys[3] = {"[7] WALK", "[8] ICE", "[9] BLOCK"};
    for (int i = 0; i < 3; i++) {
        bool selected = ((TerrainType)g_state.selectedTerrainType == terrainOrder[i]);
        Color bc = selected ? TerrainColor(terrainOrder[i]) : C_BUTTON;
        DrawRectangle(10, y, 112, 24, bc);
        DrawText(terrainKeys[i], 18, y + 6, 10, selected ? BLACK : C_TEXT);
        if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){10, y, 112, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_state.selectedTerrainType = terrainOrder[i];
        }
        y += 30;
    }
    DrawRectangle(130, y - 90, 138, 24, g_state.terrainPaintOnly ? C_ACCENT : C_BUTTON);
    DrawText("[T] TERRAIN ONLY", 138, y - 84, 10, g_state.terrainPaintOnly ? BLACK : C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){130, y - 90, 138, 24}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.terrainPaintOnly = !g_state.terrainPaintOnly;
    }
    DrawText("RMB paints terrain on existing tiles", 10, y + 2, 9, C_TEXT_DIM);
    y += 8;
    
    // Selected
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText("SELECTED:", 10, y, 11, C_ACCENT2);
    y += 20;
    
    if (g_state.selectedTileId >= 0 && g_state.selectedTileId < g_tileDefCount) {
        TileDef* t = &g_tileDefs[g_state.selectedTileId];
        DrawText(TextFormat("[%d] %s", t->id, t->name), 10, y, 10, C_TEXT);
        y += 15;
        DrawText(TextFormat("%.0fx%.0f from Tex[%d]", t->sourceRect.width, t->sourceRect.height,
                t->textureIndex), 10, y, 9, C_TEXT_DIM);
        y += 15;
        
        if (t->textureIndex >= 0 && t->textureIndex < g_textureCount &&
            g_textures[t->textureIndex].loaded) {
            float scale = 1.0f;
            if (t->sourceRect.width > 80) scale = 80.0f / t->sourceRect.width;
            DrawTexturePro(g_textures[t->textureIndex].texture, t->sourceRect,
                          (Rectangle){10, y, t->sourceRect.width * scale,
                                     t->sourceRect.height * scale},
                          (Vector2){0, 0}, 0, WHITE);
            y += (int)(t->sourceRect.height * scale) + 10;
        }
    } else {
        DrawText("Click a tile below", 10, y, 10, C_TEXT_DIM);
        y += 25;
    }
    y += 15;
    
    // All tiles with position info
    DrawLine(0, y, sw, y, C_GRID);
    y += 10;
    DrawText(TextFormat("ALL TILES (%d):", g_tileDefCount), 10, y, 11, C_ACCENT2);
    y += 15;
    DrawText("Hover for preview, click to select", 10, y, 9, C_TEXT_DIM);
    y += 12;
    DrawText("Ctrl+Click: multi | Shift+Click: range", 10, y, 9, C_TEXT_DIM);
    y += 12;
    DrawText("Ctrl+A: all visible | Enter: confirm", 10, y, 9, C_TEXT_DIM);
    y += 15;
    
    int perRow = GetSidebarTilesPerRow();
    
    for (int i = 0; i < g_tileDefCount; i++) {
        Rectangle slot = GetSidebarTileSlotRect(i);
        int tileX = (int)slot.x;
        y = (int)slot.y;
        
        // Selection highlight
        if (i == g_state.selectedTileId) {
            DrawRectangle(tileX - 2, y - 2, TILE_PREVIEW_SIZE + 4, TILE_PREVIEW_SIZE + 4, C_ACCENT);
        }

        // Multi-selection highlight (yellow border)
        if (IsTileSelected(i)) {
            DrawRectangleLines(tileX - 2, y - 2, TILE_PREVIEW_SIZE + 4, TILE_PREVIEW_SIZE + 4, YELLOW);
        }

        // Range selection highlight (semi-transparent)
        if (g_state.rangeSelecting && g_state.rangeStartIdx >= 0) {
            int rangeStart = g_state.rangeStartIdx;
            int rangeEnd = g_state.hoveredTileId >= 0 ? g_state.hoveredTileId : g_state.rangeStartIdx;
            if (rangeStart > rangeEnd) { int tmp = rangeStart; rangeStart = rangeEnd; rangeEnd = tmp; }
            if (i >= rangeStart && i <= rangeEnd) {
                DrawRectangle(tileX, y, TILE_PREVIEW_SIZE, TILE_PREVIEW_SIZE, ColorAlpha(YELLOW, 0.3f));
            }
        }

        // Draw tile
        TileDef* t = &g_tileDefs[i];
        if (t->textureIndex >= 0 && t->textureIndex < g_textureCount &&
            g_textures[t->textureIndex].loaded) {
            float maxD = t->sourceRect.width > t->sourceRect.height ?
                        t->sourceRect.width : t->sourceRect.height;
            float sc = (TILE_PREVIEW_SIZE - 4) / maxD;
            if (sc > 1) sc = 1;
            
            int pw = t->sourceRect.width * sc;
            int ph = t->sourceRect.height * sc;
            int px = tileX + (TILE_PREVIEW_SIZE - pw) / 2;
            int py = y + (TILE_PREVIEW_SIZE - ph) / 2;
            
            DrawTexturePro(g_textures[t->textureIndex].texture, t->sourceRect,
                          (Rectangle){px, py, pw, ph}, (Vector2){0, 0}, 0, WHITE);
        }
        
        DrawRectangleLines(tileX, y, TILE_PREVIEW_SIZE, TILE_PREVIEW_SIZE, C_GRID);
        
        // Tile index label
        DrawText(TextFormat("%d", i), tileX + 2, y + 2, 8, C_TEXT_DIM);
    }

    int rows = (g_tileDefCount + perRow - 1) / perRow;
    int contentBottom = GetSidebarAllTilesStartY() + rows * (TILE_PREVIEW_SIZE + 4);
    int contentH = contentBottom + 50 + g_state.sidebarScrollY;
    g_state.maxSidebarScroll = contentH > GetScreenHeight() ? contentH - GetScreenHeight() : 0;
    ClampSidebarScroll();
    
    EndScissorMode();
    
    // Click detection outside scissor mode for accurate coordinates
    if (IsMouseOverSidebar() && !g_state.showHelp && !g_state.showSlicePanel &&
        !g_state.showSaveDialog && !g_state.showOpenDialog && !g_state.showExitConfirm) {

        Vector2 m = GetMousePosition();

        int hoveredFromSidebar = GetSidebarTileAtPosition(m);

        // Update hovered tile for range preview
        if (hoveredFromSidebar >= 0) {
            g_state.hoveredTileId = hoveredFromSidebar;
        }

        // Handle Shift+Click for range selection (first click sets start, second confirms)
        if (hoveredFromSidebar >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_SHIFT)) {
            if (g_state.rangeSelecting && g_state.rangeStartIdx >= 0) {
                // Second shift+click - confirm range
                int endIdx = hoveredFromSidebar;
                SelectTileRange(g_state.rangeStartIdx, endIdx);
                g_state.rangeSelecting = false;
                g_state.rangeStartIdx = -1;
            } else {
                // First shift+click - start range selection
                g_state.rangeSelecting = true;
                g_state.rangeStartIdx = hoveredFromSidebar;
                ShowMessage(TextFormat("Range selection started at [%d] - Shift+click to end", hoveredFromSidebar), false);
            }
        }
        // Handle Ctrl/Cmd+Click for toggle selection
        else if (hoveredFromSidebar >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_MOD)) {
            ToggleTileSelection(hoveredFromSidebar);
        }
        // Normal click - select single tile and clear multi-selection
        else if (hoveredFromSidebar >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_state.selectedTileId = hoveredFromSidebar;
            g_state.mode = MODE_PAINT;
            // Clear multi-selection on normal click unless we just finished a range
            if (!g_state.rangeSelecting) {
                ClearTileSelection();
            }
            ShowMessage(TextFormat("Selected tile [%d] %s", hoveredFromSidebar, g_tileDefs[hoveredFromSidebar].name), false);
        }

        // Hover highlight
        if (hoveredFromSidebar >= 0) {
            Rectangle hit = GetSidebarTileSlotRect(hoveredFromSidebar);
            DrawRectangleLines((int)hit.x, (int)hit.y, (int)hit.width, (int)hit.height, YELLOW);
        }
    }
    
    // Scrollbar
    if (g_state.maxSidebarScroll > 0) {
        float ratio = (float)g_state.sidebarScrollY / g_state.maxSidebarScroll;
        int bh = GetScreenHeight() * GetScreenHeight() / contentH;
        if (bh < 30) bh = 30;
        int by = ratio * (GetScreenHeight() - bh);
        DrawRectangle(sw - 6, by, 4, bh, C_TEXT_DIM);
    }

    DrawRectangle(sw - 4, 0, 4, GetScreenHeight(), g_state.resizingSidebar ? C_ACCENT : C_GRID);
}

void DrawMessages(void)
{
    if (g_state.messageTimer > 0) {
        int alpha = (int)(g_state.messageTimer / 3.0f * 255);
        if (alpha > 255) alpha = 255;
        
        int w = MeasureText(g_state.message, 14) + 20;
        int h = 30;
        int x = (GetScreenWidth() - w) / 2;
        int y = GetScreenHeight() - 50;
        
        Color bg = ColorAlpha(g_state.messageIsError ? C_ERROR_BG : C_MESSAGE_BG, 
                             (float)alpha / 255.0f);
        
        DrawRectangle(x, y, w, h, bg);
        DrawRectangleLines(x, y, w, h, C_ACCENT);
        DrawText(g_state.message, x + 10, y + 8, 14, C_TEXT);
    }
}

void DrawTilePreviewPopup(void)
{
    if (!g_state.showTilePreview || g_state.hoveredTileId < 0 || 
        g_state.hoveredTileId >= g_tileDefCount || 
        g_state.showSlicePanel || g_state.showHelp || g_state.showSaveDialog || g_state.showOpenDialog || g_state.showExitConfirm) return;
    
    TileDef* t = &g_tileDefs[g_state.hoveredTileId];
    if (t->textureIndex < 0 || t->textureIndex >= g_textureCount) return;
    if (!g_textures[t->textureIndex].loaded) return;
    
    // Draw popup near mouse
    Vector2 m = GetMousePosition();
    int pw = 400;
    int ph = 300;
    int px = m.x + 20;
    int py = m.y + 20;
    
    if (px + pw > GetScreenWidth()) px = m.x - pw - 20;
    if (py + ph > GetScreenHeight()) py = m.y - ph - 20;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    
    DrawRectangle(px, py, pw, ph, C_PANEL);
    DrawRectangleLines(px, py, pw, ph, C_ACCENT);
    
    DrawText("TILE PREVIEW", px + 10, py + 10, 16, C_ACCENT);
    
    char info[256];
    snprintf(info, sizeof(info), "[%d] %s from %s", 
            t->id, t->name, GetFileName(g_textures[t->textureIndex].filename));
    DrawText(info, px + 10, py + 35, 11, C_TEXT);
    
    snprintf(info, sizeof(info), "Position: %.0f,%.0f  Size: %.0fx%.0f",
            t->sourceRect.x, t->sourceRect.y,
            t->sourceRect.width, t->sourceRect.height);
    DrawText(info, px + 10, py + 52, 10, C_TEXT_DIM);
    
    // Draw full texture with highlight
    int drawX = px + 10;
    int drawY = py + 75;
    int maxW = pw - 20;
    int maxH = ph - 85;
    
    Texture2D tex = g_textures[t->textureIndex].texture;
    if (tex.width <= 0 || tex.height <= 0) return;

    float scale = (float)maxW / tex.width;
    if (tex.height * scale > maxH) scale = (float)maxH / tex.height;
    
    int dw = tex.width * scale;
    int dh = tex.height * scale;
    int dx = drawX + (maxW - dw) / 2;
    int dy = drawY + (maxH - dh) / 2;
    
    DrawTextureEx(tex, (Vector2){dx, dy}, 0, scale, WHITE);
    
    // Highlight tile position
    int hx = dx + t->sourceRect.x * scale;
    int hy = dy + t->sourceRect.y * scale;
    int hw = t->sourceRect.width * scale;
    int hh = t->sourceRect.height * scale;
    
    DrawRectangleLines(hx, hy, hw, hh, C_ACCENT);
    DrawRectangle(hx, hy, hw, hh, ColorAlpha(C_ACCENT, 0.3f));
}

void DrawSlicePanel(void)
{
    int pw = 900, ph = 650;
    int px = (GetScreenWidth() - pw) / 2;
    int py = (GetScreenHeight() - ph) / 2;
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.8f));
    DrawRectangle(px, py, pw, ph, C_PANEL);
    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2, C_ACCENT);

    Rectangle closeRect = {px + pw - 40, py + 10, 28, 24};
    DrawRectangleRec(closeRect, C_BUTTON);
    DrawText("X", px + pw - 31, py + 16, 12, C_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showSlicePanel = false;
        if (g_state.numericEditActive) CancelNumericEdit();
        return;
    }
    
    DrawText("SLICE SPRITESHEET", px + 20, py + 15, 22, C_ACCENT);
    DrawText("ESC or S to close", px + 20, py + 42, 10, C_TEXT_DIM);
    
    int previewX = px + 20;
    int previewY = py + 70;
    int previewW = 500;
    int previewH = ph - 100;
    
    DrawRectangle(previewX, previewY, previewW, previewH, C_BG);
    DrawRectangleLines(previewX, previewY, previewW, previewH, C_GRID);
    
    if (g_state.sliceTexIndex >= 0 && g_state.sliceTexIndex < g_textureCount &&
        g_textures[g_state.sliceTexIndex].loaded) {
        
        Texture2D tex = g_textures[g_state.sliceTexIndex].texture;
        
        float scale = 1.0f;
        if (tex.width > previewW - 20) scale = (float)(previewW - 20) / tex.width;
        if (tex.height * scale > previewH - 20) scale = (float)(previewH - 20) / tex.height;
        
        int drawW = tex.width * scale;
        int drawH = tex.height * scale;
        int drawX = previewX + (previewW - drawW) / 2;
        int drawY = previewY + (previewH - drawH) / 2;
        
        DrawTextureEx(tex, (Vector2){drawX, drawY}, 0, scale, WHITE);

        Rectangle imageRect = {(float)drawX, (float)drawY, (float)drawW, (float)drawH};
        
        // Grid overlay for auto mode
        if (g_state.sliceMethod == SLICE_GRID_AUTO) {
            int tw = g_state.sliceGridW * scale;
            int th = g_state.sliceGridH * scale;
            int ox = g_state.sliceOffsetX * scale;
            int oy = g_state.sliceOffsetY * scale;
            int sx = g_state.sliceSpacingX * scale;
            int sy = g_state.sliceSpacingY * scale;
            
            for (int r = 0; r < g_state.sliceRows; r++) {
                for (int c = 0; c < g_state.sliceCols; c++) {
                    int gx = drawX + ox + c * (tw + sx);
                    int gy = drawY + oy + r * (th + sy);
                    
                    if (gx >= drawX && gy >= drawY && gx + tw <= drawX + drawW && gy + th <= drawY + drawH) {
                        DrawRectangleLines(gx, gy, tw, th, C_ACCENT);
                    }
                }
            }

            DrawText("Drag on image to move offset", drawX + 8, drawY + 8, 10, YELLOW);
            if (CheckCollisionPointRec(GetMousePosition(), imageRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                g_state.sliceDraggingOffset = true;
                g_state.sliceDragStartMouse = GetMousePosition();
                g_state.sliceDragStartOffsetX = g_state.sliceOffsetX;
                g_state.sliceDragStartOffsetY = g_state.sliceOffsetY;
            }
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                g_state.sliceDraggingOffset = false;
            }
            if (g_state.sliceDraggingOffset) {
                Vector2 dm = GetMousePosition();
                float dx = dm.x - g_state.sliceDragStartMouse.x;
                float dy = dm.y - g_state.sliceDragStartMouse.y;
                int nox = g_state.sliceDragStartOffsetX + (int)(dx / scale);
                int noy = g_state.sliceDragStartOffsetY + (int)(dy / scale);
                if (nox < 0) nox = 0;
                if (noy < 0) noy = 0;
                if (nox > 512) nox = 512;
                if (noy > 512) noy = 512;
                g_state.sliceOffsetX = nox;
                g_state.sliceOffsetY = noy;
            }
        }
        
        // Manual snap mode
        if (g_state.sliceMethod == SLICE_GRID_MANUAL) {
            Vector2 m = GetMousePosition();
            int snapX = g_state.manualSnapX * scale;
            int snapY = g_state.manualSnapY * scale;
            
            int relX = (int)(m.x - drawX);
            int relY = (int)(m.y - drawY);
            
            int snapGX = (relX / snapX) * snapX;
            int snapGY = (relY / snapY) * snapY;
            
            if (snapGX < 0) snapGX = 0;
            if (snapGY < 0) snapGY = 0;
            if (snapGX > drawW - snapX) snapGX = drawW - snapX;
            if (snapGY > drawH - snapY) snapGY = drawH - snapY;
            
            int finalX = drawX + snapGX;
            int finalY = drawY + snapGY;
            
            if (CheckCollisionPointRec(m, (Rectangle){drawX, drawY, drawW, drawH})) {
                DrawRectangle(finalX, finalY, snapX, snapY, ColorAlpha(C_ACCENT2, 0.3f));
                DrawRectangleLines(finalX, finalY, snapX, snapY, C_ACCENT2);
                DrawText(TextFormat("%dx%d", g_state.manualSnapX, g_state.manualSnapY),
                        finalX + 2, finalY + 2, 10, C_ACCENT2);
                
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Rectangle src = {
                        snapGX / scale,
                        snapGY / scale,
                        g_state.manualSnapX,
                        g_state.manualSnapY
                    };
                    AddManualTile(g_state.sliceTexIndex, src);
                }
            }
        }
    }
    
    // Controls
    int cx = px + previewW + 40;
    int cy = py + 70;
    
    DrawText("SETTINGS:", cx, cy, 14, C_ACCENT2);
    cy += 30;
    
    // Texture selector
    DrawText("Texture:", cx, cy, 11, C_TEXT);
    cy += 20;
    int pageSize = 8;
    int maxPage = (g_textureCount <= 0) ? 0 : (g_textureCount - 1) / pageSize;
    if (g_state.sliceTexturePage > maxPage) g_state.sliceTexturePage = maxPage;
    int start = g_state.sliceTexturePage * pageSize;
    int end = start + pageSize;
    if (end > g_textureCount) end = g_textureCount;
    for (int i = start; i < end; i++) {
        int local = i - start;
        Color c = (g_state.sliceTexIndex == i) ? C_ACCENT : C_BUTTON;
        DrawRectangle(cx + (local % 2) * 80, cy + (local / 2) * 30, 75, 26, c);
        DrawText(TextFormat("[%d]", i), cx + 10 + (local % 2) * 80, cy + 6 + (local / 2) * 30, 10,
                (g_state.sliceTexIndex == i) ? BLACK : C_TEXT);
        
        if (CheckCollisionPointRec(GetMousePosition(),
            (Rectangle){cx + (local % 2) * 80, cy + (local / 2) * 30, 75, 26})) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) g_state.sliceTexIndex = i;
        }
    }
    Rectangle prevPage = {cx, cy + 122, 75, 24};
    Rectangle nextPage = {cx + 84, cy + 122, 75, 24};
    DrawRectangleRec(prevPage, C_BUTTON);
    DrawText("Prev", cx + 20, cy + 129, 10, C_TEXT);
    DrawRectangleRec(nextPage, C_BUTTON);
    DrawText("Next", cx + 104, cy + 129, 10, C_TEXT);
    DrawText(TextFormat("Page %d/%d", g_state.sliceTexturePage + 1, maxPage + 1), cx + 170, cy + 129, 10, C_TEXT_DIM);
    if (CheckCollisionPointRec(GetMousePosition(), prevPage) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && g_state.sliceTexturePage > 0) g_state.sliceTexturePage--;
    if (CheckCollisionPointRec(GetMousePosition(), nextPage) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && g_state.sliceTexturePage < maxPage) g_state.sliceTexturePage++;
    cy += 160;
    
    // Method toggle
    DrawText("Method:", cx, cy, 11, C_TEXT);
    cy += 25;
    
    Color cAuto = (g_state.sliceMethod == SLICE_GRID_AUTO) ? C_ACCENT : C_BUTTON;
    Color cMan = (g_state.sliceMethod == SLICE_GRID_MANUAL) ? C_ACCENT : C_BUTTON;
    
    DrawRectangle(cx, cy, 160, 28, cAuto);
    DrawText("[1] Auto Grid", cx + 15, cy + 7, 11, (g_state.sliceMethod == SLICE_GRID_AUTO) ? BLACK : C_TEXT);
    
    DrawRectangle(cx + 170, cy, 160, 28, cMan);
    DrawText("[2] Manual Snap", cx + 185, cy + 7, 11, (g_state.sliceMethod == SLICE_GRID_MANUAL) ? BLACK : C_TEXT);

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){cx, cy, 160, 28}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.sliceMethod = SLICE_GRID_AUTO;
    }
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){cx + 170, cy, 160, 28}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.sliceMethod = SLICE_GRID_MANUAL;
    }
    
    if (!g_state.numericEditActive && IsKeyPressed(KEY_ONE)) g_state.sliceMethod = SLICE_GRID_AUTO;
    if (!g_state.numericEditActive && IsKeyPressed(KEY_TWO)) g_state.sliceMethod = SLICE_GRID_MANUAL;
    
    cy += 50;
    
    // Settings with +/- buttons and direct numeric input
    if (g_state.sliceMethod == SLICE_GRID_AUTO) {
        DrawText("Grid Settings:", cx, cy, 12, C_ACCENT2);
        cy += 25;

        DrawSpinControl("Tile W:", NUM_FIELD_SLICE_GRID_W, cx, cy, 1, 256);
        cy += 25;
        DrawSpinControl("Tile H:", NUM_FIELD_SLICE_GRID_H, cx, cy, 1, 256);
        cy += 25;
        DrawSpinControl("Cols:", NUM_FIELD_SLICE_COLS, cx, cy, 1, 32);
        cy += 25;
        DrawSpinControl("Rows:", NUM_FIELD_SLICE_ROWS, cx, cy, 1, 32);
        cy += 25;
        DrawSpinControl("Offset X:", NUM_FIELD_SLICE_OFFSET_X, cx, cy, 0, 512);
        cy += 25;
        DrawSpinControl("Offset Y:", NUM_FIELD_SLICE_OFFSET_Y, cx, cy, 0, 512);
        cy += 25;
        DrawSpinControl("Spacing X:", NUM_FIELD_SLICE_SPACING_X, cx, cy, 0, 64);
        cy += 25;
        DrawSpinControl("Spacing Y:", NUM_FIELD_SLICE_SPACING_Y, cx, cy, 0, 64);
        cy += 35;
        
        // Generate button in a fixed bottom area to avoid overlap
        Rectangle genRect = {cx, py + ph - 150, 220, 38};
        DrawRectangleRec(genRect, C_ACCENT2);
        DrawText("[G] GENERATE ALL", cx + 30, py + ph - 138, 14, BLACK);
        if ((!g_state.numericEditActive && IsKeyPressed(KEY_G)) || (CheckCollisionPointRec(GetMousePosition(), genRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            if (g_state.sliceTexIndex >= 0 && g_state.sliceTexIndex < g_textureCount) {
                GenerateTilesFromGrid(g_state.sliceTexIndex, g_state.sliceGridW, g_state.sliceGridH,
                                    g_state.sliceCols, g_state.sliceRows,
                                    g_state.sliceOffsetX, g_state.sliceOffsetY,
                                    g_state.sliceSpacingX, g_state.sliceSpacingY);
            }
        }

        // Import Full Image as Map button
        Rectangle importRect = {cx, py + ph - 105, 220, 38};
        DrawRectangleRec(importRect, ColorAlpha(ORANGE, 0.8f));
        DrawText("[I] IMPORT AS MAP", cx + 25, py + ph - 93, 14, WHITE);
        if ((!g_state.numericEditActive && IsKeyPressed(KEY_I)) || (CheckCollisionPointRec(GetMousePosition(), importRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            if (g_state.sliceTexIndex >= 0 && g_state.sliceTexIndex < g_textureCount) {
                ImportImageAsMap(g_state.sliceTexIndex, g_state.sliceGridW);
            }
        }
    }
    
    if (g_state.sliceMethod == SLICE_GRID_MANUAL) {
        DrawText("Snap Settings:", cx, cy, 12, C_ACCENT2);
        cy += 25;

        DrawSpinControl("Snap X:", NUM_FIELD_MANUAL_SNAP_X, cx, cy, 1, 256);
        cy += 25;
        DrawSpinControl("Snap Y:", NUM_FIELD_MANUAL_SNAP_Y, cx, cy, 1, 256);
        cy += 35;
        
        DrawText("Click on preview to add tiles", cx, cy, 11, C_TEXT_DIM);
        cy += 25;
        DrawText("with snap to grid", cx, cy, 11, C_TEXT_DIM);
    }
    
    // Stats
    cy = py + ph - 52;
    DrawLine(cx - 10, cy, cx + 340, cy, C_GRID);
    cy += 15;
    DrawText(TextFormat("Total tiles: %d", g_tileDefCount), cx, cy, 11, C_TEXT);
}

void DrawSaveDialog(void)
{
    int w = 540, h = 280;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    Rectangle inputRect = (Rectangle){x + 20, y + 80, 500, 35};
    Rectangle toggleRect = (Rectangle){x + 20, y + 135, 24, 24};
    Rectangle saveRect = (Rectangle){x + 100, y + 215, 120, 38};
    Rectangle cancelRect = (Rectangle){x + 320, y + 215, 120, 38};
    char resolvedName[MAX_FILENAME] = {0};
    bool validName = ResolveSaveFilename(g_mapFilename, resolvedName, sizeof(resolvedName));
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.8f));
    DrawRectangle(x, y, w, h, C_PANEL);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 2, C_ACCENT);
    
    DrawText("SAVE MAP V8", x + 20, y + 20, 20, C_ACCENT);
    DrawText("Enter filename (.c added automatically):", x + 20, y + 55, 12, C_TEXT);
    
    // Filename input
    DrawRectangleRec(inputRect, C_BG);
    DrawRectangleLines((int)inputRect.x, (int)inputRect.y, (int)inputRect.width, (int)inputRect.height, C_GRID);
    DrawText(g_mapFilename, x + 30, y + 88, 14, C_TEXT);
    DrawText(validName ? TextFormat("Output: %s", resolvedName) : "Output: invalid filename", x + 20, y + 120, 11,
             validName ? C_TEXT_DIM : RED);

    DrawRectangleLinesEx(toggleRect, 2, g_state.includeStandaloneMain ? C_ACCENT2 : C_GRID);
    if (g_state.includeStandaloneMain) {
        DrawRectangle((int)toggleRect.x + 5, (int)toggleRect.y + 5, 14, 14, C_ACCENT2);
    }
    DrawText("Include standalone raylib main() in export", x + 55, y + 138, 12, C_TEXT);
    DrawText("OFF(default): modular map API for your own main.c", x + 55, y + 156, 10, C_TEXT_DIM);
    DrawText(validName && FileExists(resolvedName) ? "This will overwrite an existing file" : "", x + 20, y + 188, 10, ORANGE);
    
    // Buttons
    DrawRectangleRec(saveRect, validName ? C_ACCENT2 : C_BUTTON_HOVER);
    DrawText("SAVE", x + 138, y + 226, 14, BLACK);
    
    DrawRectangleRec(cancelRect, C_BUTTON);
    DrawText("CANCEL", x + 336, y + 226, 14, C_TEXT);
    
    // Click handling
    if (CheckCollisionPointRec(GetMousePosition(), toggleRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.includeStandaloneMain = !g_state.includeStandaloneMain;
    }

    if (CheckCollisionPointRec(GetMousePosition(), saveRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (SaveMapAsC(g_mapFilename, g_state.includeStandaloneMain)) {
            g_state.showSaveDialog = false;
        }
    }
    
    if (CheckCollisionPointRec(GetMousePosition(), cancelRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showSaveDialog = false;
    }
    
    // Keyboard input
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(g_mapFilename);
        if (len < MAX_FILENAME - 1 && key >= 32 && key <= 126) {
            g_mapFilename[len] = (char)key;
            g_mapFilename[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    static float saveBackspaceDelay = 0.35f;
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(g_mapFilename);
        if (len > 0) g_mapFilename[len - 1] = '\0';
        saveBackspaceDelay = 0.35f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        saveBackspaceDelay -= GetFrameTime();
        if (saveBackspaceDelay <= 0.0f) {
            int len = (int)strlen(g_mapFilename);
            if (len > 0) g_mapFilename[len - 1] = '\0';
            saveBackspaceDelay = 0.05f;
        }
    } else {
        saveBackspaceDelay = 0.35f;
    }

    if (IsKeyPressed(KEY_TAB)) {
        g_state.includeStandaloneMain = !g_state.includeStandaloneMain;
    }

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) &&
        SaveMapAsC(g_mapFilename, g_state.includeStandaloneMain)) {
        g_state.showSaveDialog = false;
    }
}

void DrawExitConfirm(void)
{
    int w = 400, h = 150;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.85f));
    DrawRectangle(x, y, w, h, C_PANEL);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 2, C_ACCENT);
    
    DrawText("EXIT?", x + 30, y + 25, 22, C_ACCENT);
    DrawText("Do you want to exit the editor?", x + 30, y + 60, 12, C_TEXT);
    
    // Yes button
    DrawRectangle(x + 80, y + 100, 100, 35, C_ACCENT2);
    DrawText("YES (Y)", x + 95, y + 108, 14, BLACK);
    
    // No button
    DrawRectangle(x + 220, y + 100, 100, 35, C_BUTTON);
    DrawText("NO (N)", x + 240, y + 108, 14, C_TEXT);
    
    // Click handling
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){x + 80, y + 100, 100, 35}) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showExitConfirm = false;
        g_state.requestExit = true;
    }
    
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){x + 220, y + 100, 100, 35}) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showExitConfirm = false;
    }
    
    // Keyboard handled in HandleInput() to avoid double ESC consumption.
}

void DrawHelp(void)
{
    Rectangle r = {300, 100, 800, 600};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.85f));
    DrawRectangleRec(r, C_PANEL);
    DrawRectangleLinesEx(r, 2, C_ACCENT);
    
    DrawText("TILEMAP EDITOR v8 - HELP", r.x + 40, r.y + 40, 24, C_ACCENT);
    DrawText("Press H to close", r.x + 40, r.y + 75, 12, C_TEXT_DIM);
    
    int y = r.y + 120;
    int x1 = r.x + 50;
    int x2 = r.x + 420;
    
    char saveLine[64];
    snprintf(saveLine, sizeof(saveLine), "%s + S    Save map", MOD_SYMBOL);
    
    DrawText("GENERAL:", x1, y, 14, C_ACCENT2);
    y += 30;
    DrawText("H              Toggle help", x1, y, 12, C_TEXT); y += 22;
    DrawText("S              Toggle slice panel", x1, y, 12, C_TEXT); y += 22;
    DrawText("G              Toggle grid", x1, y, 12, C_TEXT); y += 22;
    DrawText("1 / 2 / 3      Paint / Erase / Fill", x1, y, 12, C_TEXT); y += 22;
    DrawText("7 / 8 / 9      Walkable / Ice / Blocked", x1, y, 12, C_TEXT); y += 22;
    DrawText("T              Toggle terrain-only paint", x1, y, 12, C_TEXT); y += 22;
    DrawText("ESC            Close panels / Exit confirm", x1, y, 12, C_TEXT); y += 22;
    DrawText(saveLine, x1, y, 12, C_TEXT); y += 22;
    DrawText("DEL x2         Clear selected tile (safe confirm)", x1, y, 12, C_TEXT); y += 35;
    
    DrawText("CAMERA:", x1, y, 14, C_ACCENT2);
    y += 30;
    DrawText("Middle drag / Alt+Left    Pan", x1, y, 12, C_TEXT); y += 22;
    DrawText("Wheel on canvas           Zoom", x1, y, 12, C_TEXT); y += 22;
    DrawText("Wheel on sidebar          Scroll tiles", x1, y, 12, C_TEXT); y += 22;
    DrawText("Sidebar +/- W/H           Resize map canvas", x1, y, 12, C_TEXT); y += 35;
    
    y = r.y + 120;
    DrawText("TILE PREVIEW:", x2, y, 14, C_ACCENT2);
    y += 30;
    DrawText("Hover tile in sidebar     See full texture", x2, y, 12, C_TEXT); y += 22;
    DrawText("                          with highlighted tile", x2, y, 12, C_TEXT); y += 22;
    DrawText("Click tile preview         Select for painting", x2, y, 12, C_TEXT); y += 22;
    DrawText("Shared hitboxes in v8      Fix wrong tile click", x2, y, 12, C_TEXT); y += 35;
    
    DrawText("SLICE PANEL:", x2, y, 14, C_ACCENT2);
    y += 30;
    DrawText("1 / 2                     Auto / Manual mode", x2, y, 12, C_TEXT); y += 22;
    DrawText("G                         Generate grid", x2, y, 12, C_TEXT); y += 22;
    DrawText("Click (manual)            Add snapped tile", x2, y, 12, C_TEXT); y += 22;
    DrawText("Duplicates are skipped    with message", x2, y, 12, C_TEXT); y += 22;
    DrawText("Tab in save dialog        Toggle standalone main", x2, y, 12, C_TEXT); y += 22;
    DrawText("RMB on map                Paint terrain only", x2, y, 12, C_TEXT);
}

bool SaveMapAsC(const char* filename, bool includeStandaloneMain)
{
    char resolvedName[MAX_FILENAME];
    if (!ResolveSaveFilename(filename, resolvedName, sizeof(resolvedName))) {
        ShowMessage("Invalid filename. Use a local name like my_map", true);
        return false;
    }

    FILE* f = fopen(resolvedName, "w");
    if (!f) {
        ShowMessage("Failed to save file!", true);
        return false;
    }

    fprintf(f, "/* Auto-generated tilemap from tilemap_editor_v8.c */\n");
    fprintf(f, "/* Default output is modular: include this file from your single main.c */\n");
    fprintf(f, "/* Compile with external main: gcc main.c -o game $(pkg-config --libs --cflags raylib) */\n");
    fprintf(f, "/* Optional standalone main included: %s */\n\n", includeStandaloneMain ? "yes" : "no");
    fprintf(f, "#include \"raylib.h\"\n");
    fprintf(f, "#include <stdbool.h>\n");
    fprintf(f, "#include <stdio.h>\n\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include <math.h>\n\n");

    fprintf(f, "#ifndef TERRAIN_RUNTIME_H\n");
    fprintf(f, "typedef enum { TERRAIN_WALKABLE = 0, TERRAIN_ICE = 1, TERRAIN_BLOCKED = 2, TERRAIN_TYPE_COUNT = 3 } TerrainType;\n");
    fprintf(f, "#endif\n\n");

    fprintf(f, "#define MAP_WIDTH %d\n", g_map.width);
    fprintf(f, "#define MAP_HEIGHT %d\n", g_map.height);
    fprintf(f, "#define MAP_DEFAULT_TILE_SIZE %d\n", g_state.defaultTileSize);
    fprintf(f, "#define TILE_COUNT %d\n", g_tileDefCount);
    fprintf(f, "#define TEXTURE_COUNT %d\n\n", g_textureCount);

    fprintf(f, "typedef struct {\n");
    fprintf(f, "    int textureIndex;\n");
    fprintf(f, "    Rectangle sourceRect;\n");
    fprintf(f, "    const char* name;\n");
    fprintf(f, "} TileDef;\n\n");

    fprintf(f, "static const char* TEXTURE_FILENAMES[(TEXTURE_COUNT > 0) ? TEXTURE_COUNT : 1] = {\n");
    if (g_textureCount == 0) {
        fprintf(f, "    \"\"\n");
    } else {
        for (int i = 0; i < g_textureCount; i++) {
            fprintf(f, "    ");
            WriteEscapedCString(f, GetFileName(g_textures[i].filename));
            if (i < g_textureCount - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const TileDef TILE_DEFINITIONS[(TILE_COUNT > 0) ? TILE_COUNT : 1] = {\n");
    if (g_tileDefCount == 0) {
        fprintf(f, "    {0, {0, 0, 0, 0}, \"empty\"}\n");
    }
    for (int i = 0; i < g_tileDefCount; i++) {
        TileDef* t = &g_tileDefs[i];
        fprintf(f, "    {%d, {%.0f, %.0f, %.0f, %.0f}, ",
                t->textureIndex, t->sourceRect.x, t->sourceRect.y,
                t->sourceRect.width, t->sourceRect.height);
        WriteEscapedCString(f, t->name);
        fprintf(f, "}");
        if (i < g_tileDefCount - 1) fprintf(f, ",");
        fprintf(f, "  // [%d]\n", i);
    }
    fprintf(f, "};\n\n");

    int invalidTileRefs = 0;
    int invalidTerrainRefs = 0;

    fprintf(f, "static const int MAP_DATA[MAP_HEIGHT][MAP_WIDTH] = {\n");
    for (int y = 0; y < g_map.height; y++) {
        fprintf(f, "    {");
        for (int x = 0; x < g_map.width; x++) {
            int tid = g_map.cells[y][x].tileId;
            if (tid < -1 || tid >= g_tileDefCount) {
                tid = -1;
                invalidTileRefs++;
            }
            fprintf(f, "%d", tid);
            if (x < g_map.width - 1) fprintf(f, ", ");
        }
        fprintf(f, "}");
        if (y < g_map.height - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const uint8_t TERRAIN_DATA[MAP_HEIGHT][MAP_WIDTH] = {\n");
    for (int y = 0; y < g_map.height; y++) {
        fprintf(f, "    {");
        for (int x = 0; x < g_map.width; x++) {
            int terrain = g_map.cells[y][x].terrainType;
            if (terrain < 0 || terrain >= TERRAIN_TYPE_COUNT) {
                terrain = TERRAIN_WALKABLE;
                invalidTerrainRefs++;
            }
            fprintf(f, "%d", terrain);
            if (x < g_map.width - 1) fprintf(f, ", ");
        }
        fprintf(f, "}");
        if (y < g_map.height - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "TerrainType MapGetTerrainAt(int cellX, int cellY) {\n");
    fprintf(f, "    if (cellX < 0 || cellY < 0 || cellX >= MAP_WIDTH || cellY >= MAP_HEIGHT) return TERRAIN_BLOCKED;\n");
    fprintf(f, "    return (TerrainType)TERRAIN_DATA[cellY][cellX];\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool MapIsWalkable(int cellX, int cellY) {\n");
    fprintf(f, "    TerrainType t = MapGetTerrainAt(cellX, cellY);\n");
    fprintf(f, "    return t != TERRAIN_BLOCKED;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "Vector2 MapWorldToCell(Vector2 worldPos) {\n");
    fprintf(f, "    return (Vector2){ floorf(worldPos.x / MAP_DEFAULT_TILE_SIZE), floorf(worldPos.y / MAP_DEFAULT_TILE_SIZE) };\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool MapWorldIsWalkable(Vector2 worldPos) {\n");
    fprintf(f, "    Vector2 c = MapWorldToCell(worldPos);\n");
    fprintf(f, "    return MapIsWalkable((int)c.x, (int)c.y);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void MapDrawRange(Texture2D* textures, int minX, int maxX, int minY, int maxY, float offX, float offY, float scale) {\n");
    fprintf(f, "    if (scale <= 0.0f) scale = 1.0f;\n");
    fprintf(f, "    if (minX < 0) minX = 0;\n");
    fprintf(f, "    if (minY < 0) minY = 0;\n");
    fprintf(f, "    if (maxX >= MAP_WIDTH) maxX = MAP_WIDTH - 1;\n");
    fprintf(f, "    if (maxY >= MAP_HEIGHT) maxY = MAP_HEIGHT - 1;\n");
    fprintf(f, "    for (int y = minY; y <= maxY; y++) {\n");
    fprintf(f, "        for (int x = minX; x <= maxX; x++) {\n");
    fprintf(f, "            int tid = MAP_DATA[y][x];\n");
    fprintf(f, "            if (tid < 0 || tid >= TILE_COUNT) continue;\n");
    fprintf(f, "            const TileDef* t = &TILE_DEFINITIONS[tid];\n");
    fprintf(f, "            if (t->textureIndex < 0 || t->textureIndex >= TEXTURE_COUNT) continue;\n");
    fprintf(f, "            Rectangle dst = {\n");
    fprintf(f, "                offX + x * MAP_DEFAULT_TILE_SIZE * scale,\n");
    fprintf(f, "                offY + y * MAP_DEFAULT_TILE_SIZE * scale,\n");
    fprintf(f, "                t->sourceRect.width * scale,\n");
    fprintf(f, "                t->sourceRect.height * scale\n");
    fprintf(f, "            };\n");
    fprintf(f, "            if (textures[t->textureIndex].id != 0) DrawTexturePro(textures[t->textureIndex], t->sourceRect, dst, (Vector2){0, 0}, 0, WHITE);\n");
    fprintf(f, "            else { DrawRectangleRec(dst, MAGENTA); DrawRectangleLinesEx(dst, 1.0f, BLACK); }\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void MapDraw(Texture2D* textures, float offX, float offY, float scale) {\n");
    fprintf(f, "    MapDrawRange(textures, 0, MAP_WIDTH - 1, 0, MAP_HEIGHT - 1, offX, offY, scale);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void MapDrawCulled(Texture2D* textures, Camera2D camera, int screenWidth, int screenHeight, float offX, float offY, float scale) {\n");
    fprintf(f, "    if (scale <= 0.0f) scale = 1.0f;\n");
    fprintf(f, "    float cellW = MAP_DEFAULT_TILE_SIZE * scale;\n");
    fprintf(f, "    if (cellW < 1.0f) cellW = 1.0f;\n");
    fprintf(f, "    Vector2 w0 = GetScreenToWorld2D((Vector2){0, 0}, camera);\n");
    fprintf(f, "    Vector2 w1 = GetScreenToWorld2D((Vector2){(float)screenWidth, (float)screenHeight}, camera);\n");
    fprintf(f, "    float minWX = (w0.x < w1.x) ? w0.x : w1.x;\n");
    fprintf(f, "    float maxWX = (w0.x > w1.x) ? w0.x : w1.x;\n");
    fprintf(f, "    float minWY = (w0.y < w1.y) ? w0.y : w1.y;\n");
    fprintf(f, "    float maxWY = (w0.y > w1.y) ? w0.y : w1.y;\n");
    fprintf(f, "    int minX = (int)floorf((minWX - offX) / cellW) - 1;\n");
    fprintf(f, "    int maxX = (int)ceilf((maxWX - offX) / cellW) + 1;\n");
    fprintf(f, "    int minY = (int)floorf((minWY - offY) / cellW) - 1;\n");
    fprintf(f, "    int maxY = (int)ceilf((maxWY - offY) / cellW) + 1;\n");
    fprintf(f, "    MapDrawRange(textures, minX, maxX, minY, maxY, offX, offY, scale);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool MapLoadTextures(Texture2D* textures) {\n");
    fprintf(f, "    bool allLoaded = true;\n");
    fprintf(f, "    for (int i = 0; i < TEXTURE_COUNT; i++) {\n");
    fprintf(f, "        textures[i] = LoadTexture(TEXTURE_FILENAMES[i]);\n");
    fprintf(f, "        if (textures[i].id == 0) {\n");
    fprintf(f, "            fprintf(stderr, \"Failed to load texture: %%s\\n\", TEXTURE_FILENAMES[i]);\n");
    fprintf(f, "            allLoaded = false;\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "    return allLoaded;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void MapUnloadTextures(Texture2D* textures) {\n");
    fprintf(f, "    for (int i = 0; i < TEXTURE_COUNT; i++) {\n");
    fprintf(f, "        if (textures[i].id != 0) UnloadTexture(textures[i]);\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n\n");

    if (includeStandaloneMain) {
        int screenWidth = g_map.width * g_state.defaultTileSize + 240;
        int screenHeight = g_map.height * g_state.defaultTileSize + 180;
        if (screenWidth < 960) screenWidth = 960;
        if (screenHeight < 640) screenHeight = 640;

        fprintf(f, "int main(void) {\n");
        fprintf(f, "    const int screenWidth = %d;\n", screenWidth);
        fprintf(f, "    const int screenHeight = %d;\n", screenHeight);
        fprintf(f, "    SetConfigFlags(FLAG_WINDOW_RESIZABLE);\n");
        fprintf(f, "    InitWindow(screenWidth, screenHeight, \"Exported Tilemap\");\n");
        fprintf(f, "    SetTargetFPS(60);\n\n");
        fprintf(f, "    Texture2D textures[(TEXTURE_COUNT > 0) ? TEXTURE_COUNT : 1] = {0};\n");
        fprintf(f, "    bool allTexturesLoaded = MapLoadTextures(textures);\n\n");
        fprintf(f, "    Camera2D camera = {0};\n");
        fprintf(f, "    camera.zoom = 1.0f;\n");
        fprintf(f, "    camera.offset = (Vector2){screenWidth / 2.0f, screenHeight / 2.0f};\n");
        fprintf(f, "    camera.target = (Vector2){(MAP_WIDTH * MAP_DEFAULT_TILE_SIZE) / 2.0f, (MAP_HEIGHT * MAP_DEFAULT_TILE_SIZE) / 2.0f};\n\n");
        fprintf(f, "    Vector2 player = {(float)(MAP_DEFAULT_TILE_SIZE * 2), (float)(MAP_DEFAULT_TILE_SIZE * 2)};\n");
        fprintf(f, "    Vector2 velocity = {0};\n\n");
        fprintf(f, "    while (!WindowShouldClose()) {\n");
        fprintf(f, "        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) camera.target.x += 5.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) camera.target.x -= 5.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) camera.target.y += 5.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) camera.target.y -= 5.0f;\n");
        fprintf(f, "        Vector2 in = {0};\n");
        fprintf(f, "        if (IsKeyDown(KEY_RIGHT)) in.x += 1.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_LEFT)) in.x -= 1.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_DOWN)) in.y += 1.0f;\n");
        fprintf(f, "        if (IsKeyDown(KEY_UP)) in.y -= 1.0f;\n");
        fprintf(f, "        Vector2 cell = MapWorldToCell(player);\n");
        fprintf(f, "        TerrainType terrain = MapGetTerrainAt((int)cell.x, (int)cell.y);\n");
        fprintf(f, "        float friction = (terrain == TERRAIN_ICE) ? 0.985f : 0.80f;\n");
        fprintf(f, "        velocity.x = velocity.x * friction + in.x * 0.6f;\n");
        fprintf(f, "        velocity.y = velocity.y * friction + in.y * 0.6f;\n");
        fprintf(f, "        Vector2 next = { player.x + velocity.x, player.y + velocity.y };\n");
        fprintf(f, "        if (MapWorldIsWalkable(next)) player = next;\n");
        fprintf(f, "        else velocity = (Vector2){0};\n");
        fprintf(f, "        float wheel = GetMouseWheelMove();\n");
        fprintf(f, "        if (wheel != 0.0f) {\n");
        fprintf(f, "            camera.zoom += wheel * 0.1f;\n");
        fprintf(f, "            if (camera.zoom < 0.1f) camera.zoom = 0.1f;\n");
        fprintf(f, "            if (camera.zoom > 6.0f) camera.zoom = 6.0f;\n");
        fprintf(f, "        }\n\n");
        fprintf(f, "        BeginDrawing();\n");
        fprintf(f, "        ClearBackground((Color){28, 28, 34, 255});\n");
        fprintf(f, "        BeginMode2D(camera);\n");
        fprintf(f, "        MapDrawCulled(textures, camera, screenWidth, screenHeight, 0, 0, 1.0f);\n");
        fprintf(f, "        DrawCircleV(player, 10, MAROON);\n");
        fprintf(f, "        DrawRectangleLines(0, 0, MAP_WIDTH * MAP_DEFAULT_TILE_SIZE, MAP_HEIGHT * MAP_DEFAULT_TILE_SIZE, GREEN);\n");
        fprintf(f, "        EndMode2D();\n\n");
        fprintf(f, "        DrawText(\"Exported map preview\", 16, 16, 24, GREEN);\n");
        fprintf(f, "        DrawText(\"WASD/Arrows move, wheel zoom\", 16, 46, 18, RAYWHITE);\n");
        fprintf(f, "        DrawText(TextFormat(\"Map: %%dx%%d  Tiles: %%d  Textures: %%d\", MAP_WIDTH, MAP_HEIGHT, TILE_COUNT, TEXTURE_COUNT), 16, 72, 18, LIGHTGRAY);\n");
        fprintf(f, "        if (!allTexturesLoaded) DrawText(\"Some textures are missing. Placeholder magenta tiles are shown.\", 16, 98, 18, ORANGE);\n");
        fprintf(f, "        EndDrawing();\n");
        fprintf(f, "    }\n\n");
        fprintf(f, "    MapUnloadTextures(textures);\n");
        fprintf(f, "    CloseWindow();\n");
        fprintf(f, "    return 0;\n");
        fprintf(f, "}\n");
    }

    fclose(f);
    g_state.hasUnsavedChanges = false;
    if (invalidTileRefs > 0 || invalidTerrainRefs > 0) {
        ShowMessage(TextFormat("Saved with cleanup (%d invalid tile refs, %d invalid terrain refs)", invalidTileRefs, invalidTerrainRefs), true);
    } else {
        ShowMessage(TextFormat("Saved to %s", resolvedName), false);
    }
    return true;
}

bool LoadMapFromC(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        ShowMessage(TextFormat("Failed to open: %s", filename), true);
        return false;
    }

    // Clear current state
    for (int i = 0; i < g_textureCount; i++) {
        if (g_textures[i].loaded) UnloadTexture(g_textures[i].texture);
    }
    g_textureCount = 0;
    g_tileDefCount = 0;

    char line[1024];
    int parsingSection = 0;

    // Parse defines first
    int mapW = 20, mapH = 15, tileSize = 32, tileCount = 0, texCount = 0;

    while (fgets(line, sizeof(line), f)) {
        // Parse #define values
        if (sscanf(line, "#define MAP_WIDTH %d", &mapW) == 1) continue;
        if (sscanf(line, "#define MAP_HEIGHT %d", &mapH) == 1) continue;
        if (sscanf(line, "#define MAP_DEFAULT_TILE_SIZE %d", &tileSize) == 1) continue;
        if (sscanf(line, "#define TILE_COUNT %d", &tileCount) == 1) continue;
        if (sscanf(line, "#define TEXTURE_COUNT %d", &texCount) == 1) continue;

        // Parse texture filenames from array
        if (strstr(line, "TEXTURE_FILENAMES[") && strstr(line, " = {")) {
            parsingSection = 1;
            continue;
        }
        if (parsingSection == 1) {
            char texFile[256];
            if (sscanf(line, " \"%255[^\"]\",", texFile) == 1) {
                if (g_textureCount < MAX_TEXTURES) {
                    LoadTextureFile(texFile);
                }
            }
            if (strstr(line, "};") || strstr(line, "} ;")) {
                parsingSection = 0;
            }
        }

        // Parse tile definitions from TILE_DEFINITIONS
        if (strstr(line, "TILE_DEFINITIONS[") && strstr(line, " = {")) {
            parsingSection = 2;
            continue;
        }
        if (parsingSection == 2) {
            int texIdx;
            float x, y, w, h;
            char name[MAX_TILE_NAME];
            if (sscanf(line, " {%d, {%f, %f, %f, %f}, \"%31[^\"]\"}", &texIdx, &x, &y, &w, &h, name) == 6) {
                if (g_tileDefCount < MAX_TILES) {
                    TileDef* t = &g_tileDefs[g_tileDefCount];
                    t->id = g_tileDefCount;
                    t->textureIndex = texIdx;
                    t->sourceRect = (Rectangle){x, y, w, h};
                    strncpy(t->name, name, MAX_TILE_NAME - 1);
                    t->name[MAX_TILE_NAME - 1] = '\0';
                    g_tileDefCount++;
                }
            }
            if (strstr(line, "};") || strstr(line, "} ;")) {
                parsingSection = 0;
            }
        }

        // Parse map data from MAP_DATA
        static int currentRow = 0;
        if (strstr(line, "MAP_DATA[") && strstr(line, " = {")) {
            parsingSection = 3;
            currentRow = 0;
            if (mapW < 1) mapW = 1;
            if (mapH < 1) mapH = 1;
            if (mapW > MAX_MAP_WIDTH) mapW = MAX_MAP_WIDTH;
            if (mapH > MAX_MAP_HEIGHT) mapH = MAX_MAP_HEIGHT;
            ResizeMap(mapW, mapH);
            g_state.defaultTileSize = tileSize;
            g_map.defaultTileSize = tileSize;
            continue;
        }
        if (parsingSection == 3) {
            char* p = strchr(line, '{');
            if (p) {
                p++;
                int col = 0;
                int row = currentRow;
                while (*p && col < g_map.width) {
                    int tileId;
                    if (sscanf(p, "%d", &tileId) == 1) {
                        if (tileId < -1 || tileId >= g_tileDefCount) tileId = -1;
                        if (row >= 0 && row < MAX_MAP_HEIGHT && col >= 0 && col < MAX_MAP_WIDTH) {
                            g_map.cells[row][col].tileId = tileId;
                            g_map.cells[row][col].terrainType = TERRAIN_WALKABLE;
                            g_map.cells[row][col].ownerX = (tileId >= 0) ? col : -1;
                            g_map.cells[row][col].ownerY = (tileId >= 0) ? row : -1;
                        }
                        col++;
                    }
                    while (*p && (*p != ',' && *p != '}')) p++;
                    if (*p == ',') p++;
                    while (*p && (*p == ' ' || *p == '\t')) p++;
                }
                currentRow++;
            }
            if (strstr(line, "};") || strstr(line, "} ;")) {
                parsingSection = 0;
            }
        }

        // Parse terrain data from TERRAIN_DATA
        static int terrainRow = 0;
        if (strstr(line, "TERRAIN_DATA[") && strstr(line, " = {")) {
            parsingSection = 4;
            terrainRow = 0;
            continue;
        }
        if (parsingSection == 4) {
            char* p = strchr(line, '{');
            if (p) {
                p++;
                int col = 0;
                int row = terrainRow;
                while (*p && col < g_map.width) {
                    int terrain;
                    if (sscanf(p, "%d", &terrain) == 1) {
                        if (row >= 0 && row < MAX_MAP_HEIGHT && col >= 0 && col < MAX_MAP_WIDTH) {
                            if (terrain < TERRAIN_WALKABLE || terrain >= TERRAIN_TYPE_COUNT) {
                                terrain = TERRAIN_WALKABLE;
                            }
                            g_map.cells[row][col].terrainType = (uint8_t)terrain;
                        }
                        col++;
                    }
                    while (*p && (*p != ',' && *p != '}')) p++;
                    if (*p == ',') p++;
                    while (*p && (*p == ' ' || *p == '\t')) p++;
                }
                terrainRow++;
            }
            if (strstr(line, "};") || strstr(line, "} ;")) {
                parsingSection = 0;
            }
        }
    }

    fclose(f);

    RebuildOwnershipFromAnchors();

    // Reset camera
    g_camera.target = (Vector2){0, 0};
    g_camera.zoom = 1.0f;
    g_state.hasUnsavedChanges = false;

    ShowMessage(TextFormat("Loaded %d tiles from %s", g_tileDefCount, filename), false);
    return true;
}

void DrawOpenDialog(void)
{
    int w = 540, h = 240;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    Rectangle inputRect = (Rectangle){x + 20, y + 80, 500, 35};
    Rectangle openRect = (Rectangle){x + 100, y + 175, 120, 38};
    Rectangle cancelRect = (Rectangle){x + 320, y + 175, 120, 38};

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.8f));
    DrawRectangle(x, y, w, h, C_PANEL);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 2, C_ACCENT);

    DrawText("OPEN MAP FILE", x + 20, y + 20, 20, C_ACCENT);
    DrawText("Enter .c filename to load:", x + 20, y + 55, 12, C_TEXT);

    // Filename input
    DrawRectangleRec(inputRect, C_BG);
    DrawRectangleLines((int)inputRect.x, (int)inputRect.y, (int)inputRect.width, (int)inputRect.height, C_GRID);
    DrawText(g_state.openFilename, x + 30, y + 88, 14, C_TEXT);

    bool fileExists = FileExists(g_state.openFilename);
    DrawText(fileExists ? "File found" : "File not found", x + 20, y + 125, 11, fileExists ? C_ACCENT2 : RED);

    // Buttons
    DrawRectangleRec(openRect, fileExists ? C_ACCENT2 : C_BUTTON_HOVER);
    DrawText("OPEN", x + 138, y + 186, 14, BLACK);

    DrawRectangleRec(cancelRect, C_BUTTON);
    DrawText("CANCEL", x + 336, y + 186, 14, C_TEXT);

    // Click handling
    if (CheckCollisionPointRec(GetMousePosition(), openRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (fileExists) {
            if (LoadMapFromC(g_state.openFilename)) {
                strncpy(g_mapFilename, g_state.openFilename, MAX_FILENAME - 1);
                g_mapFilename[MAX_FILENAME - 1] = '\0';
                g_state.showOpenDialog = false;
            }
        }
    }

    if (CheckCollisionPointRec(GetMousePosition(), cancelRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_state.showOpenDialog = false;
    }

    // Keyboard input
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(g_state.openFilename);
        if (len < MAX_FILENAME - 1 && key >= 32 && key <= 126) {
            g_state.openFilename[len] = (char)key;
            g_state.openFilename[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    // Backspace
    static float openBackspaceDelay = 0.35f;
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(g_state.openFilename);
        if (len > 0) g_state.openFilename[len - 1] = '\0';
        openBackspaceDelay = 0.35f;
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        openBackspaceDelay -= GetFrameTime();
        if (openBackspaceDelay <= 0.0f) {
            int len = (int)strlen(g_state.openFilename);
            if (len > 0) g_state.openFilename[len - 1] = '\0';
            openBackspaceDelay = 0.05f;
        }
    } else {
        openBackspaceDelay = 0.35f;
    }

    // Enter to open
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (fileExists) {
            if (LoadMapFromC(g_state.openFilename)) {
                strncpy(g_mapFilename, g_state.openFilename, MAX_FILENAME - 1);
                g_mapFilename[MAX_FILENAME - 1] = '\0';
                g_state.showOpenDialog = false;
            }
        }
    }
}

// Import full image as map - converts each grid cell to a tile and places on map
void ImportImageAsMap(int texIdx, int tileSize)
{
    if (texIdx < 0 || texIdx >= g_textureCount) return;
    if (!g_textures[texIdx].loaded) return;
    if (tileSize <= 0) tileSize = 32;

    Texture2D tex = g_textures[texIdx].texture;
    int imgW = tex.width;
    int imgH = tex.height;

    // Calculate grid dimensions
    int cols = imgW / tileSize;
    int rows = imgH / tileSize;

    if (cols <= 0 || rows <= 0) {
        ShowMessage("Image too small for import", true);
        return;
    }

    if (cols > MAX_MAP_WIDTH || rows > MAX_MAP_HEIGHT) {
        ShowMessage("Image too large for map", true);
        return;
    }

    // Resize map to fit the image
    ResizeMap(cols, rows);

    // Track how many tiles we create
    int tilesCreated = 0;
    int tilesPlaced = 0;

    // For each grid cell, create a tile and place it on the map
    for (int row = 0; row < rows && g_tileDefCount < MAX_TILES; row++) {
        for (int col = 0; col < cols && g_tileDefCount < MAX_TILES; col++) {
            Rectangle rect = {
                (float)(col * tileSize),
                (float)(row * tileSize),
                (float)tileSize,
                (float)tileSize
            };

            // Check if this exact tile already exists
            int existingTileId = -1;
            for (int i = 0; i < g_tileDefCount; i++) {
                TileDef* t = &g_tileDefs[i];
                if (t->textureIndex == texIdx &&
                    (int)t->sourceRect.x == (int)rect.x &&
                    (int)t->sourceRect.y == (int)rect.y &&
                    (int)t->sourceRect.width == (int)rect.width &&
                    (int)t->sourceRect.height == (int)rect.height) {
                    existingTileId = i;
                    break;
                }
            }

            int tileId;
            if (existingTileId >= 0) {
                tileId = existingTileId;
            } else {
                // Create new tile definition
                TileDef* t = &g_tileDefs[g_tileDefCount];
                t->id = g_tileDefCount;
                t->textureIndex = texIdx;
                t->sourceRect = rect;
                snprintf(t->name, MAX_TILE_NAME, "import_%s_%d_%d",
                        GetFileNameWithoutExt(g_textures[texIdx].filename), row, col);
                tileId = g_tileDefCount;
                g_tileDefCount++;
                tilesCreated++;
            }

            // Place tile on map at (col, row)
            if (col >= 0 && col < g_map.width && row >= 0 && row < g_map.height) {
                ClearTileAtAnchor(col, row, true);
                if (CanPlaceTileAt(tileId, col, row)) {
                    ApplyTileAt(tileId, col, row);
                    tilesPlaced++;
                }
            }
        }
    }

    MarkMapDirty();
    ShowMessage(TextFormat("Imported: %d tiles created, %d placed", tilesCreated, tilesPlaced), false);
}
