#include <switch.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    printf("Aleph One Switch bootstrap (Phase 1)\n");
    printf("This validates libnx + SDL2 toolchain wiring.\n");
    printf("Press + to exit.\n");

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
                break;
            }
            consoleUpdate(NULL);
        }
        consoleExit(NULL);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Aleph One Switch Bootstrap",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_SHOWN);

    SDL_Renderer *renderer = NULL;
    if (window) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }

    bool running = true;
    while (running && appletMainLoop()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            running = false;
        }

        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 12, 18, 28, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }

        consoleUpdate(NULL);
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
    consoleExit(NULL);
    return 0;
}
