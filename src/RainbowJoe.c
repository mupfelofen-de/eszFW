/**
 * @file RainbowJoe.c
 * @ingroup RainbowJoe
 * @defgroup RainbowJoe
 * @author Michael Fitzmayer
 * @copyright "THE BEER-WARE LICENCE" (Revision 42)
 */

#define QUIT_FAILURE { sExecStatus = EXIT_FAILURE; goto quit; }
#define QUIT_SUCCESS { sExecStatus = EXIT_SUCCESS; goto quit; }

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AABB.h"
#include "Entity.h"
#include "Font.h"
#include "FPSLimiter.h"
#include "Input.h"
#include "Macros.h"
#include "Map.h"
#include "RainbowJoe.h"
#include "Video.h"

int InitGame()
{
    int     sExecStatus = EXIT_SUCCESS;
    Camera *pstCamera   = NULL;
    Entity *pstPlayer   = NULL;
    Font   *pstFont     = NULL;
    Map    *pstMap      = NULL;
    Object *pstPlrSpawn = NULL;
    Sprite *pstSprite   = NULL;
    Video  *pstVideo    = NULL;
    bool    bDebug      = true;
    double  dTimeA      = 0.f;

    if (-1 == InitVideo(256, 240, false, &pstVideo))           { QUIT_FAILURE; }
    if (-1 == InitCamera(&pstCamera))                          { QUIT_FAILURE; }
    if (-1 == InitEntity(0, 0, &pstPlayer))                    { QUIT_FAILURE; }
    if (-1 == InitMap("res/maps/Demo.tmx", &pstMap))           { QUIT_FAILURE; }
    if (-1 == InitObject(&pstPlrSpawn))                        { QUIT_FAILURE; }
    if (-1 == InitFont("res/ttf/FifteenNarrow.ttf", &pstFont)) { QUIT_FAILURE; }

    GetSingleObjectByName("Player", &pstMap, &pstPlrSpawn);
    SetPosition(pstPlrSpawn->dPosX, pstPlrSpawn->dPosY, &pstPlayer);
    SetFrameOffset(0, 2, &pstPlayer);
    SetFontColour(255, 255, 255, &pstFont);

    if (-1 == InitSprite(
            "res/images/characters_7.png", 736, 128, 0, 0,
            &pstSprite, &pstVideo->pstRenderer))
    {
        QUIT_FAILURE;
    }

    InitFPSLimiter(&dTimeA);
    while (1)
    {
        double dTimeB;
        double dDeltaTime;
        LimitFramerate(60, &dTimeA, &dTimeB, &dDeltaTime);

        const uint8_t *pu8KeyState;
        if (-1 == ReadInput(&pu8KeyState) || pu8KeyState[SDL_SCANCODE_Q])
        {
            QUIT_SUCCESS;
        }

        CLEAR(pstPlayer->u16Flags, IS_IN_MID_AIR);
        CLEAR(pstPlayer->u16Flags, IS_WALKING);

        if (pu8KeyState[SDL_SCANCODE_LSHIFT])
        {
            if (pu8KeyState[SDL_SCANCODE_D]) { bDebug = true; }
        }
        else
        {
            if (pu8KeyState[SDL_SCANCODE_D]) { bDebug = false; }
        }

        if (pu8KeyState[SDL_SCANCODE_0])
        {
            SetZoomLevel(pstVideo->dInitialZoomLevel, &pstVideo);
        }
        if (pu8KeyState[SDL_SCANCODE_1])
        {
            SetZoomLevel(pstVideo->dZoomLevel + 0.01, &pstVideo);
        }
        if (pu8KeyState[SDL_SCANCODE_2])
        {
            SetZoomLevel(pstVideo->dZoomLevel - 0.01, &pstVideo);
        }

        if (pu8KeyState[SDL_SCANCODE_SPACE])
        {
            if ((IS_NOT_SET(pstPlayer->u16Flags, IS_JUMPING)) &&
                (IS_NOT_SET(pstPlayer->u16Flags, IS_IN_MID_AIR)))
            {
                SET(pstPlayer->u16Flags, IS_JUMPING);
            }
        }

        if (pu8KeyState[SDL_SCANCODE_LEFT])
        {
            SET(pstPlayer->u16Flags, IS_WALKING);
            SetOrientation(LEFT, &pstPlayer);
            SetAnimation(0, 3, &pstPlayer);
        }
        if (pu8KeyState[SDL_SCANCODE_RIGHT])
        {
            SET(pstPlayer->u16Flags, IS_WALKING);
            SetOrientation(RIGHT, &pstPlayer);
            SetAnimation(0, 3, &pstPlayer);
        }

        SetCameraTargetEntity(
            pstVideo->s32WindowWidth,
            pstVideo->s32WindowHeight,
            pstVideo->dZoomLevel,
            &pstCamera,
            &pstPlayer);

        SetCameraBoundariesToMapSize(
            pstVideo->s32WindowWidth,
            pstVideo->s32WindowHeight,
            pstVideo->dZoomLevel,
            pstMap->u16Width,
            pstMap->u16Height,
            &pstCamera);

        // Set up collision detection.
        if (false == IsOnPlatform(pstPlayer->dPosX, pstPlayer->dPosY, 18.0, &pstMap))
        {
            SET(pstPlayer->u16Flags, IS_IN_MID_AIR);
        }
        UpdateEntity(dDeltaTime, pstMap->dGravitation, &pstPlayer);

        if (-1 == DrawMap(
                "res/images/tileset.png", false, 0, "BG",
                pstCamera->dPosX, pstCamera->dPosY, &pstMap, &pstVideo->pstRenderer))
        {
            QUIT_FAILURE;
        }

        ConnectMapEndsForEntity(pstMap->u16Width, pstMap->u16Height, &pstPlayer);
        DrawEntity(&pstPlayer, &pstCamera, &pstSprite, &pstVideo->pstRenderer);

        if (-1 == DrawMap(
                "res/images/tileset.png", false, 1, "FG",
                pstCamera->dPosX, pstCamera->dPosY, &pstMap, &pstVideo->pstRenderer))
        {
            QUIT_FAILURE;
        }

        if (bDebug)
        {
            PrintText("X:  ", 8, 4,  &pstVideo->pstRenderer, &pstFont);
            PrintText("Y:  ", 8, 20, &pstVideo->pstRenderer, &pstFont);
            PrintText("HV: ", 8, 36, &pstVideo->pstRenderer, &pstFont);
            PrintText("VV: ", 8, 52, &pstVideo->pstRenderer, &pstFont);
            PrintNumber((int32_t)round(pstPlayer->dPosX),      32,  4, &pstVideo->pstRenderer, &pstFont);
            PrintNumber((int32_t)round(pstPlayer->dPosY),      32, 20, &pstVideo->pstRenderer, &pstFont);
            PrintNumber((int32_t)round(pstPlayer->dVelocityX), 32, 36, &pstVideo->pstRenderer, &pstFont);
            PrintNumber((int32_t)round(pstPlayer->dVelocityY), 32, 52, &pstVideo->pstRenderer, &pstFont);
        }

        RenderScene(&pstVideo->pstRenderer);
    }

quit:
    FreeSprite(&pstSprite);
    FreeFont(&pstFont);
    FreeObject(&pstPlrSpawn);
    FreeMap(&pstMap);
    FreeVideo(&pstVideo);
    return sExecStatus;
}

void QuitGame()
{
    SDL_Quit();
}
