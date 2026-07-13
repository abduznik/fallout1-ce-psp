#include "plib/gnw/svga.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/winmain.h"

#ifdef __PSP__
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
// Debug screen is initted early in winmain.cc; we just call pspDebugScreenPrintf here.
#endif

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// screen rect
Rect scr_size;

// 0x6ACA18
ScreenBlitFunc* scr_blit = GNW95_ShowRect;

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlSurface = NULL;
SDL_Renderer* gSdlRenderer = NULL;
SDL_Texture* gSdlTexture = NULL;
SDL_Surface* gSdlTextureSurface = NULL;

// TODO: Remove once migration to update-render cycle is completed.
FpsLimiter sharedFpsLimiter;

// 0x4CB310
void GNW95_SetPaletteEntries(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB568
void GNW95_SetPalette(unsigned char* palette)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB850
void GNW95_ShowRect(unsigned char* src, unsigned int srcPitch, unsigned int a3, unsigned int srcX, unsigned int srcY, unsigned int srcWidth, unsigned int srcHeight, unsigned int destX, unsigned int destY)
{
    buf_to_buf(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

bool svga_init(VideoOptions* video_options)
{
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

    if (video_options->fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }

    gSdlWindow = SDL_CreateWindow(GNW95_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        video_options->width * video_options->scale,
        video_options->height * video_options->scale,
        windowFlags);
    if (gSdlWindow == NULL) {
        return false;
    }

    if (!createRenderer(video_options->width, video_options->height)) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;

        return false;
    }

    gSdlSurface = SDL_CreateRGBSurface(0,
        video_options->width,
        video_options->height,
        8,
        0,
        0,
        0,
        0);
    if (gSdlSurface == NULL) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_Color colors[256];
    for (int index = 0; index < 256; index++) {
        colors[index].r = index;
        colors[index].g = index;
        colors[index].b = index;
        colors[index].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);

    scr_size.ulx = 0;
    scr_size.uly = 0;
    scr_size.lrx = video_options->width - 1;
    scr_size.lry = video_options->height - 1;

    mouse_blit_trans = NULL;
    scr_blit = GNW95_ShowRect;
    mouse_blit = GNW95_ShowRect;

    return true;
}

void svga_exit()
{
    destroyRenderer();

    if (gSdlWindow != NULL) {
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&scr_size);
}

static bool createRenderer(int width, int height)
{
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (gSdlRenderer == NULL) {
        return false;
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        return false;
    }

    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }

    if (gSdlTexture != NULL) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = NULL;
    }

    if (gSdlRenderer != NULL) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = NULL;
    }
}

void handleWindowSizeChanged()
{
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
#ifdef __PSP__
    // One-shot PSP debug dump: writes format info to file on ms0:
    // and also attempts on-screen output via pspDebugScreenPrintf.
    static bool pspDebugDone = false;
    if (!pspDebugDone) {
        pspDebugDone = true;

        // --- File output (most reliable) ---
        // Use PSP native file API to write debug info to memory stick.
        char dbgBuf[1024];
        int dbgLen = 0;

        // Texture format info
        Uint32 texFmt = gSdlTextureSurface ? gSdlTextureSurface->format->format : 0;
        Uint32 surFmt = gSdlSurface ? gSdlSurface->format->format : 0;
        Uint16 palCount = (gSdlSurface && gSdlSurface->format->palette) ? gSdlSurface->format->palette->ncolors : 0;
        int texBpp = gSdlTextureSurface ? gSdlTextureSurface->format->BytesPerPixel : 0;
        int texW = gSdlTextureSurface ? gSdlTextureSurface->w : 0;
        int texH = gSdlTextureSurface ? gSdlTextureSurface->h : 0;

        dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
            "=== FALLOUT1-CE PSP DEBUG ===\n"
            "gSdlTextureSurface: %dx%d bpp=%d fmt=0x%x\n"
            "gSdlSurface: fmt=0x%x palette_colors=%d\n"
            "SDL_GetError: %s\n"
            "RENDERER=%p TEXTURE=%p\n",
            texW, texH, texBpp, (unsigned)texFmt,
            (unsigned)surFmt, (int)palCount,
            SDL_GetError() ? SDL_GetError() : "(null)",
            (void*)gSdlRenderer, (void*)gSdlTexture);

        SceUID fd = sceIoOpen("ms0:/psp_debug.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
        if (fd >= 0) {
            sceIoWrite(fd, dbgBuf, dbgLen);

            // Also dump first 16 palette entries
            if (gSdlSurface && gSdlSurface->format->palette) {
                char palBuf[512];
                int palOff = 0;
                for (int i = 0; i < 16 && i < (int)gSdlSurface->format->palette->ncolors; i++) {
                    SDL_Color* c = &gSdlSurface->format->palette->colors[i];
                    palOff += snprintf(palBuf + palOff, sizeof(palBuf) - palOff,
                        "pal[%2d] = R=%3d G=%3d B=%3d A=%3d\n",
                        i, c->r, c->g, c->b, c->a);
                }
                sceIoWrite(fd, palBuf, palOff);
            }

            sceIoClose(fd);
        }

        // --- On-screen text via pspDebugScreen (may be overwritten by SDL2) ---
        // Already initted early in winmain.cc, so we just print.
        pspDebugScreenPrintf("FALLOUT1-CE PSP v0.1\n");
        pspDebugScreenPrintf("Tex: %dx%d bpp=%d fmt=0x%08x\n", texW, texH, texBpp, (unsigned)texFmt);
        pspDebugScreenPrintf("Pal: %d entries\n", (int)palCount);
        if (gSdlSurface && gSdlSurface->format->palette && palCount > 0) {
            pspDebugScreenPrintf("pal[0]: R=%d G=%d B=%d\n",
                gSdlSurface->format->palette->colors[0].r,
                gSdlSurface->format->palette->colors[0].g,
                gSdlSurface->format->palette->colors[0].b);
            pspDebugScreenPrintf("pal[1]: R=%d G=%d B=%d\n",
                gSdlSurface->format->palette->colors[1].r,
                gSdlSurface->format->palette->colors[1].g,
                gSdlSurface->format->palette->colors[1].b);
        }
    }
#endif

    // Log return codes of SDL_UpdateTexture and SDL_RenderCopy for debugging
    int updateResult = SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    int copyResult = SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);

#ifdef __PSP__
    // Log errors from SDL calls (one-shot after first frame to avoid spam)
    static bool sdlErrorLogged = false;
    if (!sdlErrorLogged && (updateResult != 0 || copyResult != 0)) {
        sdlErrorLogged = true;
        const char* err = SDL_GetError();
        if (err && err[0]) {
            SceUID fd = sceIoOpen("ms0:/psp_debug.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
            if (fd >= 0) {
                char buf[256];
                int len = snprintf(buf, sizeof(buf),
                    "SDL error after frame1: update=%d copy=%d err='%s'\n",
                    updateResult, copyResult, err);
                sceIoWrite(fd, buf, len);
                sceIoClose(fd);
            }
        }
    }
#endif
}

} // namespace fallout
