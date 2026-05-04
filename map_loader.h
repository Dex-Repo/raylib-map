#ifndef MAP_LOADER_H
#define MAP_LOADER_H

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MAP_WIDTH 100
#define MAX_MAP_HEIGHT 100
#define MAX_TILES 256
#define MAX_TEXTURES 4
#define MAX_FILENAME 256

typedef struct {
    int tileId;
    int textureIndex;
} MapCell;

typedef struct {
    MapCell cells[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    int width;
    int height;
} TileMap;

typedef struct {
    int id;
    int textureIndex;
    Rectangle sourceRect;
    char name[32];
} TileDef;

typedef struct {
    Texture2D texture;
    char filename[MAX_FILENAME];
    int tileSize;
    bool loaded;
} Tileset;

// Function prototypes
bool LoadMapFromFile(const char* filename, TileMap* map, TileDef* tileDefs, int* tileDefCount, 
                     Tileset* tilesets, int* tilesetCount);
void DrawMap(TileMap* map, TileDef* tileDefs, Tileset* tilesets, int offsetX, int offsetY, float scale);
void UnloadTilesets(Tileset* tilesets, int count);

// Implementation
#ifdef MAP_LOADER_IMPLEMENTATION

bool LoadMapFromFile(const char* filename, TileMap* map, TileDef* tileDefs, int* tileDefCount,
                     Tileset* tilesets, int* tilesetCount)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to load map: %s\n", filename);
        return false;
    }
    
    // Read header
    fread(&map->width, sizeof(int), 1, file);
    fread(&map->height, sizeof(int), 1, file);
    fread(tileDefCount, sizeof(int), 1, file);
    
    // Read tile definitions
    for (int i = 0; i < *tileDefCount; i++) {
        fread(&tileDefs[i], sizeof(TileDef), 1, file);
    }
    
    // Read tileset info
    fread(tilesetCount, sizeof(int), 1, file);
    for (int i = 0; i < *tilesetCount; i++) {
        fread(tilesets[i].filename, sizeof(char), MAX_FILENAME, file);
        fread(&tilesets[i].tileSize, sizeof(int), 1, file);
        
        // Load texture
        if (FileExists(tilesets[i].filename)) {
            tilesets[i].texture = LoadTexture(tilesets[i].filename);
            tilesets[i].loaded = true;
            printf("Loaded texture: %s\n", tilesets[i].filename);
        } else {
            printf("Warning: Texture not found: %s\n", tilesets[i].filename);
            tilesets[i].loaded = false;
        }
    }
    
    // Read map data
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            fread(&map->cells[y][x], sizeof(MapCell), 1, file);
        }
    }
    
    fclose(file);
    printf("Map loaded: %s (%dx%d)\n", filename, map->width, map->height);
    return true;
}

void DrawMap(TileMap* map, TileDef* tileDefs, Tileset* tilesets, int offsetX, int offsetY, float scale)
{
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int tileId = map->cells[y][x].tileId;
            int texIndex = map->cells[y][x].textureIndex;
            
            if (tileId >= 0 && texIndex >= 0 && texIndex < MAX_TEXTURES && tilesets[texIndex].loaded) {
                TileDef* def = &tileDefs[tileId];
                int drawX = offsetX + (int)(x * def->sourceRect.width * scale);
                int drawY = offsetY + (int)(y * def->sourceRect.height * scale);
                
                Rectangle dest = {
                    drawX, drawY,
                    def->sourceRect.width * scale,
                    def->sourceRect.height * scale
                };
                
                DrawTexturePro(tilesets[texIndex].texture, def->sourceRect, dest, (Vector2){0, 0}, 0, WHITE);
            }
        }
    }
}

void UnloadTilesets(Tileset* tilesets, int count)
{
    for (int i = 0; i < count; i++) {
        if (tilesets[i].loaded) {
            UnloadTexture(tilesets[i].texture);
            tilesets[i].loaded = false;
        }
    }
}

#endif // MAP_LOADER_IMPLEMENTATION

#endif // MAP_LOADER_H
