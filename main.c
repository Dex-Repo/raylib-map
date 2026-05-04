#include "raylib.h"
#include "raymath.h"
#include "terrain_runtime.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include("build-rmp/bin/my_map.c")
#    include "build-rmp/bin/my_map.c"
#  else
#    include "my_map.c"
#  endif
#else
#  include "my_map.c"
#endif

#define RUNTIME_PATH_MAX 512

static bool IsUsableString(const char* text)
{
    return text != NULL && text[0] != '\0';
}

static int GetRequiredTextureSlots(void)
{
    int highestIndex = -1;
    for (int i = 0; i < TILE_COUNT; i++) {
        if (TILE_DEFINITIONS[i].textureIndex > highestIndex) {
            highestIndex = TILE_DEFINITIONS[i].textureIndex;
        }
    }

    int count = TEXTURE_COUNT;
    if (highestIndex + 1 > count) {
        count = highestIndex + 1;
    }
    if (count < 1) count = 1;
    return count;
}

static const char* GetFallbackTextureName(void)
{
    for (int i = 0; i < TEXTURE_COUNT; i++) {
        if (IsUsableString(TEXTURE_FILENAMES[i])) {
            return TEXTURE_FILENAMES[i];
        }
    }
    return NULL;
}

static bool ResolveAssetPath(const char* filename, char* output, size_t outputSize)
{
    static const char* prefixes[] = {
        "",
        "./",
        "assets/",
        "./assets/",
        "../",
        "../assets/",
        "../../",
        "../../assets/",
        "../Resources/",
        "../../Resources/",
        "../../../Resources/"
    };

    if (!IsUsableString(filename) || !output || outputSize == 0) return false;

    for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); i++) {
        snprintf(output, outputSize, "%s%s", prefixes[i], filename);
        if (FileExists(output)) {
            return true;
        }
    }

    output[0] = '\0';
    return false;
}

static void TrySetWindowIconRuntime(void)
{
    char iconPath[RUNTIME_PATH_MAX];
    if (!ResolveAssetPath("logo.png", iconPath, sizeof(iconPath))) {
        printf("Warning: logo.png not found for window icon\n");
        return;
    }

    Image icon = LoadImage(iconPath);
    if (icon.data != NULL) {
        SetWindowIcon(icon);
        UnloadImage(icon);
        printf("Icon loaded from: %s\n", iconPath);
    }
}

static bool LoadRuntimeTextures(Texture2D* textures, char loadedPaths[][RUNTIME_PATH_MAX], int slotCount)
{
    const char* fallbackName = GetFallbackTextureName();
    bool allLoaded = true;

    for (int i = 0; i < slotCount; i++) {
        char resolvedPath[RUNTIME_PATH_MAX];
        const char* requestedName = (i < TEXTURE_COUNT && IsUsableString(TEXTURE_FILENAMES[i]))
            ? TEXTURE_FILENAMES[i]
            : fallbackName;

        textures[i] = (Texture2D){0};
        loadedPaths[i][0] = '\0';

        if (!IsUsableString(requestedName)) {
            fprintf(stderr, "Missing texture filename for slot %d\n", i);
            allLoaded = false;
            continue;
        }

        if (!ResolveAssetPath(requestedName, resolvedPath, sizeof(resolvedPath))) {
            fprintf(stderr, "Failed to resolve texture for slot %d: %s\n", i, requestedName);
            allLoaded = false;
            continue;
        }

        textures[i] = LoadTexture(resolvedPath);
        if (textures[i].id == 0) {
            fprintf(stderr, "Failed to load texture for slot %d: %s\n", i, resolvedPath);
            allLoaded = false;
            continue;
        }

        strncpy(loadedPaths[i], resolvedPath, RUNTIME_PATH_MAX - 1);
        loadedPaths[i][RUNTIME_PATH_MAX - 1] = '\0';
        printf("Loaded texture slot %d from: %s\n", i, loadedPaths[i]);
    }

    return allLoaded;
}

static void UnloadRuntimeTextures(Texture2D* textures, int slotCount)
{
    for (int i = 0; i < slotCount; i++) {
        if (textures[i].id != 0) {
            UnloadTexture(textures[i]);
            textures[i] = (Texture2D){0};
        }
    }
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    const int textureSlotCount = GetRequiredTextureSlots();

    Texture2D textures[textureSlotCount];
    char loadedPaths[textureSlotCount][RUNTIME_PATH_MAX];

    memset(textures, 0, sizeof(textures));
    memset(loadedPaths, 0, sizeof(loadedPaths));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Raylib Map Runtime - Terrain + Collision");
    TrySetWindowIconRuntime();
    SetTargetFPS(60);

    bool allTexturesLoaded = LoadRuntimeTextures(textures, loadedPaths, textureSlotCount);

    Camera2D camera = {0};
    float mapPixelWidth = MAP_WIDTH * MAP_DEFAULT_TILE_SIZE;
    float mapPixelHeight = MAP_HEIGHT * MAP_DEFAULT_TILE_SIZE;

    camera.target = (Vector2){mapPixelWidth * 0.5f, mapPixelHeight * 0.5f};
    camera.offset = (Vector2){screenWidth * 0.5f, screenHeight * 0.5f};

    float zoomX = (screenWidth * 0.9f) / mapPixelWidth;
    float zoomY = (screenHeight * 0.9f) / mapPixelHeight;
    camera.zoom = (zoomX < zoomY) ? zoomX : zoomY;
    if (camera.zoom > 2.0f) camera.zoom = 2.0f;
    if (camera.zoom < 0.1f) camera.zoom = 0.1f;

    printf("Map size: %dx%d tiles (%dx%d pixels)\n", MAP_WIDTH, MAP_HEIGHT, (int)mapPixelWidth, (int)mapPixelHeight);
    printf("Camera zoom: %.2f, target: (%.0f, %.0f)\n", camera.zoom, camera.target.x, camera.target.y);

    TerrainMover player;
    TerrainMoverInit(&player, (Vector2){mapPixelWidth * 0.5f, mapPixelHeight * 0.5f});

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        Vector2 input = {0};
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) input.x += 1.0f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) input.x -= 1.0f;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) input.y -= 1.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) input.y += 1.0f;

        TerrainUpdateMover(&player, input, dt, MAP_DEFAULT_TILE_SIZE, MapGetTerrainAt, MapIsWalkable);

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 delta = GetMouseDelta();
            camera.target = Vector2Subtract(camera.target, Vector2Scale(delta, 1.0f / camera.zoom));
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            camera.zoom += wheel * 0.1f;
            if (camera.zoom < 0.2f) camera.zoom = 0.2f;
            if (camera.zoom > 5.0f) camera.zoom = 5.0f;
        }

        if (IsKeyPressed(KEY_R)) {
            player.position = (Vector2){mapPixelWidth * 0.5f, mapPixelHeight * 0.5f};
            player.velocity = (Vector2){0};
        }

        Vector2 cell = TerrainWorldToCell(player.position, MAP_DEFAULT_TILE_SIZE);
        TerrainType terrain = MapGetTerrainAt((int)cell.x, (int)cell.y);

        BeginDrawing();
        ClearBackground((Color){24, 24, 30, 255});

        BeginMode2D(camera);
        MapDrawCulled(textures, camera, GetScreenWidth(), GetScreenHeight(), 0, 0, 1.0f);
        DrawCircleV(player.position, player.radius, MAROON);
        DrawCircleLines((int)player.position.x, (int)player.position.y, player.radius, BLACK);
        DrawRectangleLines(0, 0, MAP_WIDTH * MAP_DEFAULT_TILE_SIZE, MAP_HEIGHT * MAP_DEFAULT_TILE_SIZE, GREEN);
        EndMode2D();

        DrawRectangle(10, 10, 520, 154, ColorAlpha(BLACK, 0.55f));
        DrawRectangleLines(10, 10, 520, 154, DARKGREEN);
        DrawText("Arrow Keys / WASD: Move", 20, 22, 20, RAYWHITE);
        DrawText("Mouse Wheel: Zoom | MMB Drag: Pan", 20, 48, 20, RAYWHITE);
        DrawText("R: Reset player position", 20, 74, 20, RAYWHITE);
        DrawText(TextFormat("Cell: (%d,%d) Terrain: %s", (int)cell.x, (int)cell.y, TerrainTypeToString(terrain)), 20, 100, 20, YELLOW);
        DrawText(TextFormat("Texture slots used: %d", textureSlotCount), 20, 126, 20, SKYBLUE);
        if (!allTexturesLoaded) {
            DrawText("Warning: some textures were repaired or failed to load", 20, 150, 18, ORANGE);
        }

        EndDrawing();
    }

    UnloadRuntimeTextures(textures, textureSlotCount);
    CloseWindow();
    return 0;
}
