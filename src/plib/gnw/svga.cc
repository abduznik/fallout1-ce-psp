#include "plib/gnw/svga.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/winmain.h"

#ifdef __PSP__
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
// File I/O debug: writes one-shot dump at svga_init time.
// pspDebugScreen is NOT used — it conflicts with SDL2's sceGu setup.
#endif

namespace fallout {

// Debug helper for PSP: appends a message to the debug file.
// Only active on PSP builds.
#ifdef __PSP__
static void psp_debug_log(const char* msg) {
    SceUID fd = sceIoOpen("ms0:/psp_debug.txt",
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoClose(fd);
    }
}
#else
#define psp_debug_log(msg) ((void)0)
#endif

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
#ifdef __PSP__
SDL_Surface* gSdlWindowSurface = NULL;
#endif

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

#ifdef __PSP__
        {
            SceUID fd = sceIoOpen("ms0:/palette_log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
            if (fd >= 0) {
                char buf[1024];
                int n = snprintf(buf, sizeof(buf),
                    "GNW95_SetPaletteEntries start=%d count=%d\n"
                    "  gSdlSurface: w=%d h=%d pitch=%d fmt=0x%x pal=%p\n"
                    "  gSdlTextureSurface: w=%d h=%d pitch=%d fmt=0x%x\n"
                    "  colors[0]: r=%d g=%d b=%d  colors[127]: r=%d g=%d b=%d\n",
                    start, count,
                    gSdlSurface->w, gSdlSurface->h, gSdlSurface->pitch, gSdlSurface->format->format, (void*)gSdlSurface->format->palette,
                    gSdlTextureSurface->w, gSdlTextureSurface->h, gSdlTextureSurface->pitch, gSdlTextureSurface->format->format,
                    colors[0].r, colors[0].g, colors[0].b,
                    count > 127 ? colors[127].r : -1, count > 127 ? colors[127].g : -1, count > 127 ? colors[127].b : -1);
                sceIoWrite(fd, buf, n);
                sceIoClose(fd);
            }
        }
#endif

        int blitRet = SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);

#ifdef __PSP__
        {
            SceUID fd = sceIoOpen("ms0:/palette_log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
            if (fd >= 0) {
                char buf[512];
                int n = snprintf(buf, sizeof(buf),
                    "  fullBlit ret=%d err=\"%s\"\n",
                    blitRet, SDL_GetError() ? SDL_GetError() : "(null)");
                sceIoWrite(fd, buf, n);
                sceIoClose(fd);
            }
        }
#endif
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
    int showBlitRet = SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
#ifdef __PSP__
    if (showBlitRet != 0) {
        SceUID fd = sceIoOpen("ms0:/showrect_error.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
        if (fd >= 0) {
            char buf[512];
            int n = snprintf(buf, sizeof(buf),
                "GNW95_ShowRect: SDL_BlitSurface ret=%d err=\"%s\"\n"
                "  srcRect(%d,%d %dx%d) dest(%d,%d)\n"
                "  src: w=%d h=%d pitch=%d fmt=0x%x\n"
                "  dst: w=%d h=%d pitch=%d fmt=0x%x\n",
                showBlitRet, SDL_GetError() ? SDL_GetError() : "(null)",
                srcRect.x, srcRect.y, srcRect.w, srcRect.h, destRect.x, destRect.y,
                gSdlSurface->w, gSdlSurface->h, gSdlSurface->pitch, gSdlSurface->format->format,
                gSdlTextureSurface->w, gSdlTextureSurface->h, gSdlTextureSurface->pitch, gSdlTextureSurface->format->format);
            sceIoWrite(fd, buf, n);
            sceIoClose(fd);
        }
    }
#endif
}

bool svga_init(VideoOptions* video_options)
{
    psp_debug_log("svga_init: SDL_SetHint...\n");
#ifndef __PSP__
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif

    psp_debug_log("svga_init: SDL_InitSubSystem...\n");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        psp_debug_log("svga_init: SDL_InitSubSystem FAILED\n");
        return false;
    }
    psp_debug_log("svga_init: SDL_InitSubSystem OK\n");

    Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __PSP__
    // PSP has no OpenGL support; use surface-based window.
    windowFlags = 0;
#else
    windowFlags |= SDL_WINDOW_OPENGL;
#endif

    if (video_options->fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }

    psp_debug_log("svga_init: SDL_CreateWindow...\n");
    gSdlWindow = SDL_CreateWindow(GNW95_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        video_options->width * video_options->scale,
        video_options->height * video_options->scale,
        windowFlags);
    if (gSdlWindow == NULL) {
        psp_debug_log("svga_init: SDL_CreateWindow FAILED\n");
        return false;
    }
    psp_debug_log("svga_init: SDL_CreateWindow OK\n");

    psp_debug_log("svga_init: createRenderer...\n");
    if (!createRenderer(video_options->width, video_options->height)) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;

        psp_debug_log("svga_init: createRenderer FAILED\n");
        return false;
    }
    psp_debug_log("svga_init: createRenderer OK\n");

    psp_debug_log("svga_init: SDL_CreateRGBSurface 8-bit...\n");
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

        psp_debug_log("svga_init: SDL_CreateRGBSurface FAILED\n");
    }
    psp_debug_log("svga_init: 8-bit surface OK\n");

    // ... (rest unchanged)

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

#ifdef __PSP__
    // One-shot debug dump at init time (MUST execute, unlike renderPresent
    // which may never be called if a system dialog blocks the render loop).
    {
        char dbgBuf[2048];
        int dbgLen = 0;
        Uint32 texFmt = gSdlTextureSurface ? gSdlTextureSurface->format->format : 0;
        Uint32 surFmt = gSdlSurface ? gSdlSurface->format->format : 0;
        int texBpp = gSdlTextureSurface ? gSdlTextureSurface->format->BytesPerPixel : 0;
        int texW = gSdlTextureSurface ? gSdlTextureSurface->w : 0;
        int texH = gSdlTextureSurface ? gSdlTextureSurface->h : 0;

        dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
            "=== FALLOUT1-CE PSP INIT ===\n"
            "gSdlTextureSurface: %dx%d bpp=%d fmt=0x%x\n"
            "gSdlSurface: fmt=0x%x\n"
            "SDL_GetError: %s\n"
            "RENDERER=%p TEXTURE=%p\n",
            texW, texH, texBpp, (unsigned)texFmt,
            (unsigned)surFmt,
            SDL_GetError() ? SDL_GetError() : "(null)",
            (void*)gSdlRenderer, (void*)gSdlTexture);

        SceUID fd = sceIoOpen("ms0:/psp_debug.txt",
            PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
        if (fd >= 0) {
            sceIoWrite(fd, dbgBuf, dbgLen);
            sceIoClose(fd);
        }
    }
#endif

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
#ifdef __PSP__
    // In-memory buffer for logging inside createRenderer.
    static char crBuf[2048];
    int crPos = 0;
#define CRLOG(fmt, ...) do { \
    int n = snprintf(crBuf + crPos, sizeof(crBuf) - crPos - 1, fmt, ##__VA_ARGS__); \
    if (n > 0) crPos += n < (int)(sizeof(crBuf) - crPos) ? n : (int)(sizeof(crBuf) - crPos - 1); \
} while(0)
#else
#define CRLOG(fmt, ...) ((void)0)
#endif

    CRLOG("  createRenderer: enter\n");
    const char* errBefore = SDL_GetError();
    CRLOG("  createRenderer: SDL_GetError before=\"%s\"\n", errBefore ? errBefore : "(null)");

#ifdef __PSP__
    // PSP: SDL2 renderer limits textures to 512x512 (hardware constraint of
    // the PSP's Media Engine GPU). Our 640x480 framebuffer exceeds this.
    // Two-step surface approach:
    //   1. gSdlTextureSurface = 640x480 RGB565 intermediate for palette conversion
    //   2. gSdlWindowSurface  = 480x272 RGB565 window surface (SDL_GetWindowSurface)
    //   3. renderPresent: SDL_BlitScaled(gSdlTextureSurface → gSdlWindowSurface)
    CRLOG("  createRenderer: PSP path — two-step surface\n");
    gSdlRenderer = NULL;
    gSdlTexture = NULL;
    gSdlWindowSurface = SDL_GetWindowSurface(gSdlWindow);
    if (gSdlWindowSurface == NULL) {
        CRLOG("  createRenderer: SDL_GetWindowSurface FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: window surface fmt=0x%x w=%d h=%d bpp=%d\n",
        (unsigned)gSdlWindowSurface->format->format,
        gSdlWindowSurface->w, gSdlWindowSurface->h,
        gSdlWindowSurface->format->BytesPerPixel);
    // Create 640x480 RGB565 intermediate surface for palette conversion + blit target
    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height,
        gSdlWindowSurface->format->BytesPerPixel * 8,
        gSdlWindowSurface->format->format);
    if (gSdlTextureSurface == NULL) {
        CRLOG("  createRenderer: SDL_CreateRGBSurfaceWithFormat FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: intermediate surface fmt=0x%x w=%d h=%d\n",
        (unsigned)gSdlTextureSurface->format->format,
        gSdlTextureSurface->w, gSdlTextureSurface->h);
#else
    // Non-PSP: standard renderer path
    CRLOG("  createRenderer: SDL_CreateRenderer...\n");
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (gSdlRenderer == NULL) {
        CRLOG("  createRenderer: SDL_CreateRenderer FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: SDL_CreateRenderer OK\n");

    CRLOG("  createRenderer: SDL_RenderSetLogicalSize...\n");
    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        CRLOG("  createRenderer: SDL_RenderSetLogicalSize FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: SDL_RenderSetLogicalSize OK\n");

    CRLOG("  createRenderer: SDL_CreateTexture RGB888...\n");
    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        CRLOG("  createRenderer: SDL_CreateTexture FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: SDL_CreateTexture OK\n");

    CRLOG("  createRenderer: SDL_QueryTexture...\n");
    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        CRLOG("  createRenderer: SDL_QueryTexture FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: SDL_QueryTexture OK format=0x%x\n", (unsigned)format);

    CRLOG("  createRenderer: SDL_CreateRGBSurfaceWithFormat...\n");
    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height,
        SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        CRLOG("  createRenderer: SDL_CreateRGBSurfaceWithFormat FAILED err=\"%s\"\n",
            SDL_GetError() ? SDL_GetError() : "(null)");
        goto cr_dump_fail;
    }
    CRLOG("  createRenderer: SDL_CreateRGBSurfaceWithFormat OK\n");
#endif

#ifdef __PSP__
    { SceUID fd = sceIoOpen("ms0:/create_renderer_debug.txt",
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
      if (fd >= 0) { sceIoWrite(fd, crBuf, strlen(crBuf)); sceIoClose(fd); } }
#endif
    return true;

cr_dump_fail:
#ifdef __PSP__
    { SceUID fd = sceIoOpen("ms0:/create_renderer_debug.txt",
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
      if (fd >= 0) { sceIoWrite(fd, crBuf, strlen(crBuf)); sceIoClose(fd); } }
#endif
    return false;
#undef CRLOG
}

static void destroyRenderer()
{
#ifdef __PSP__
    // Free the intermediate surface we allocated, but NOT the window
    // surface (SDL_DestroyWindow owns it).
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }
    gSdlWindowSurface = NULL;  // owned by window
    gSdlTexture = NULL;
    gSdlRenderer = NULL;
#else
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
#endif
}

void handleWindowSizeChanged()
{
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
#ifdef __PSP__
    // Step 1: Scale the 640x480 RGB565 intermediate surface down to
    // the 480x272 RGB565 window surface.
    int blitRet = SDL_BlitScaled(gSdlTextureSurface, NULL, gSdlWindowSurface, NULL);
    if (blitRet != 0) {
        // Log the error synchronously (before exit, so it's captured)
        SceUID fd = sceIoOpen("ms0:/render_error.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd >= 0) {
            char buf[256];
            int n = snprintf(buf, sizeof(buf),
                "renderPresent: SDL_BlitScaled returned %d\n"
                "SDL_GetError: %s\n"
                "src: %dx%d fmt=0x%x\n"
                "dst: %dx%d fmt=0x%x\n",
                blitRet,
                SDL_GetError() ? SDL_GetError() : "(null)",
                gSdlTextureSurface->w, gSdlTextureSurface->h,
                (unsigned)gSdlTextureSurface->format->format,
                gSdlWindowSurface->w, gSdlWindowSurface->h,
                (unsigned)gSdlWindowSurface->format->format);
            sceIoWrite(fd, buf, n);
            sceIoClose(fd);
        }
    }
    // Step 2: Push the window surface to the display.
    SDL_UpdateWindowSurface(gSdlWindow);

    // One-shot pixel dump (only for first 10 frames, to avoid log spam)
    {
        static int frameCount = 0;
        if (frameCount < 10) {
            SceUID fd = sceIoOpen("ms0:/pixel_dump.txt",
                PSP_O_WRONLY | PSP_O_CREAT | (frameCount == 0 ? PSP_O_TRUNC : PSP_O_APPEND), 0777);
            if (fd >= 0) {
                char buf[2048];
                int pitch = gSdlTextureSurface->pitch;
                int bpp = gSdlTextureSurface->format->BytesPerPixel;
                Uint8* px = (Uint8*)gSdlTextureSurface->pixels;
                int n = snprintf(buf, sizeof(buf),
                    "=== Frame %d ===\n"
                    "  interSurface: %dx%d pitch=%d bpp=%d fmt=0x%x\n"
                    "  winSurface: %dx%d pitch=%d fmt=0x%x\n"
                    "  Pixels[0,0]=0x%04x  [320,240]=0x%04x  [639,479]=0x%04x\n"
                    "  Pixels[100,100]=0x%04x  [200,200]=0x%04x  [400,300]=0x%04x\n",
                    frameCount,
                    gSdlTextureSurface->w, gSdlTextureSurface->h,
                    pitch, bpp, (unsigned)gSdlTextureSurface->format->format,
                    gSdlWindowSurface->w, gSdlWindowSurface->h, gSdlWindowSurface->pitch,
                    (unsigned)gSdlWindowSurface->format->format,
                    *(Uint16*)(px + 0*pitch + 0*bpp),
                    *(Uint16*)(px + 240*pitch + 320*bpp),
                    *(Uint16*)(px + 479*pitch + 639*bpp),
                    *(Uint16*)(px + 100*pitch + 100*bpp),
                    *(Uint16*)(px + 200*pitch + 200*bpp),
                    *(Uint16*)(px + 300*pitch + 400*bpp));
                sceIoWrite(fd, buf, n);
                sceIoClose(fd);
                frameCount++;
            }
        }
    }
#else
    int updateResult = SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    int copyResult = SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
#endif
}

} // namespace fallout
