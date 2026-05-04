#ifndef TERRAIN_RUNTIME_H
#define TERRAIN_RUNTIME_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>

typedef enum {
    TERRAIN_WALKABLE = 0,
    TERRAIN_ICE = 1,
    TERRAIN_BLOCKED = 2,
    TERRAIN_TYPE_COUNT = 3
} TerrainType;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float radius;
    float maxSpeed;
    float acceleration;
    float walkFriction;
    float iceFriction;
} TerrainMover;

static inline const char* TerrainTypeToString(TerrainType terrain)
{
    switch (terrain) {
        case TERRAIN_WALKABLE: return "walkable";
        case TERRAIN_ICE: return "ice";
        case TERRAIN_BLOCKED: return "blocked";
        default: return "unknown";
    }
}

static inline bool TerrainIsWalkable(TerrainType terrain)
{
    return terrain != TERRAIN_BLOCKED;
}

static inline bool TerrainIsIce(TerrainType terrain)
{
    return terrain == TERRAIN_ICE;
}

static inline Vector2 TerrainWorldToCell(Vector2 worldPos, int tileSize)
{
    if (tileSize <= 0) tileSize = 1;
    return (Vector2){
        floorf(worldPos.x / tileSize),
        floorf(worldPos.y / tileSize)
    };
}

static inline void TerrainMoverInit(TerrainMover* mover, Vector2 startPos)
{
    if (!mover) return;

    mover->position = startPos;
    mover->velocity = (Vector2){0.0f, 0.0f};
    mover->radius = 10.0f;
    mover->maxSpeed = 220.0f;
    mover->acceleration = 900.0f;
    mover->walkFriction = 0.80f;
    mover->iceFriction = 0.965f;
}

static inline bool TerrainCanMoveCircle(
    Vector2 worldPos,
    float radius,
    int tileSize,
    bool (*isCellWalkable)(int, int))
{
    if (!isCellWalkable) return false;

    Vector2 checks[5] = {
        worldPos,
        (Vector2){worldPos.x - radius, worldPos.y},
        (Vector2){worldPos.x + radius, worldPos.y},
        (Vector2){worldPos.x, worldPos.y - radius},
        (Vector2){worldPos.x, worldPos.y + radius}
    };

    for (int i = 0; i < 5; i++) {
        Vector2 cell = TerrainWorldToCell(checks[i], tileSize);
        if (!isCellWalkable((int)cell.x, (int)cell.y)) return false;
    }

    return true;
}

static inline void TerrainUpdateMover(
    TerrainMover* mover,
    Vector2 inputAxis,
    float dt,
    int tileSize,
    TerrainType (*getTerrainAt)(int, int),
    bool (*isCellWalkable)(int, int))
{
    if (!mover || !getTerrainAt || !isCellWalkable) return;

    if (dt <= 0.0f) dt = 1.0f/60.0f;

    Vector2 cell = TerrainWorldToCell(mover->position, tileSize);
    TerrainType terrain = getTerrainAt((int)cell.x, (int)cell.y);
    float friction = TerrainIsIce(terrain) ? mover->iceFriction : mover->walkFriction;

    Vector2 dir = inputAxis;
    float len = Vector2Length(dir);
    if (len > 1.0f) dir = Vector2Scale(dir, 1.0f/len);

    mover->velocity.x = mover->velocity.x * friction + dir.x * mover->acceleration * dt;
    mover->velocity.y = mover->velocity.y * friction + dir.y * mover->acceleration * dt;

    float speed = Vector2Length(mover->velocity);
    if (speed > mover->maxSpeed) {
        mover->velocity = Vector2Scale(Vector2Normalize(mover->velocity), mover->maxSpeed);
    }

    Vector2 next = {
        mover->position.x + mover->velocity.x * dt,
        mover->position.y + mover->velocity.y * dt
    };

    Vector2 nextX = {next.x, mover->position.y};
    Vector2 nextY = {mover->position.x, next.y};

    if (TerrainCanMoveCircle(nextX, mover->radius, tileSize, isCellWalkable)) {
        mover->position.x = nextX.x;
    } else {
        mover->velocity.x = 0.0f;
    }

    if (TerrainCanMoveCircle(nextY, mover->radius, tileSize, isCellWalkable)) {
        mover->position.y = nextY.y;
    } else {
        mover->velocity.y = 0.0f;
    }
}

#endif
