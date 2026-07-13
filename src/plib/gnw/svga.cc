#include "plib/gnw/svga.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/winmain.h"

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
    // DEBUG: one-shot palette + surface dump on first frame
    static bool dumpDone = false;
    if (!dumpDone) {
        dumpDone = true;
#ifdef __PSP__
        // Write to memory stick file (PSP filesystem)
        FILE* f = fopen("ms0:/PSP/GAME/FOUT00001/debug.txt", "w");
#else
        FILE* f = fopen("debug.txt", "w");
#endif
        if (f == NULL) {
            // Fallback: write to stderr (PPSSPP captures this in log)
            f = stderr;
            fprintf(f, "=== DEBUG DUMP (stderr fallback) ===\n");
        }
        if (f) {
            // Dump first 16 palette entries
            fprintf(f, "=== PALETTE (first 16 of 256) ===\n");
            if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
                SDL_Palette* pal = gSdlSurface->format->palette;
                for (int i = 0; i < 16 && i < (int)pal->ncolors; i++) {
                    fprintf(f, "  [%3d] R=%3d G=%3d B=%3d A=%3d\n",
                            i, pal->colors[i].r, pal->colors[i].g,
                            pal->colors[i].b, pal->colors[i].a);
                }
            } else {
                fprintf(f, "  gSdlSurface or palette is NULL!\n");
            }

            // Dump raw pixel indices from gSdlSurface at various positions
            fprintf(f, "\n=== gSdlSurface 8-bit pixel indices ===\n");
            if (gSdlSurface != NULL && gSdlSurface->pixels != NULL) {
                int w = gSdlSurface->w;
                int h = gSdlSurface->h;
                fprintf(f, "  surface: %dx%d, pitch=%d, bpp=%d\n",
                        w, h, gSdlSurface->pitch, gSdlSurface->format->BitsPerPixel);
                // Sample a grid of pixels
                int positions[][2] = {
                    {0,0}, {w/4,0}, {w/2,0}, {3*w/4,0},
                    {0,h/4}, {w/4,h/4}, {w/2,h/4}, {3*w/4,h/4},
                    {0,h/2}, {w/4,h/2}, {w/2,h/2}, {3*w/4,h/2},
                    {0,3*h/4}, {w/4,3*h/4}, {w/2,3*h/4}, {3*w/4,3*h/4},
                };
                Uint8* pixels8 = (Uint8*)gSdlSurface->pixels;
                for (int i = 0; i < 16; i++) {
                    int x = positions[i][0];
                    int y = positions[i][1];
                    Uint8 idx = pixels8[y * gSdlSurface->pitch + x];
                    fprintf(f, "  pixel[%4d,%4d] = index %3d", x, y, idx);
                    if (gSdlSurface->format->palette != NULL && idx < gSdlSurface->format->palette->ncolors) {
                        fprintf(f, "  -> RGB(%3d,%3d,%3d)\n",
                                gSdlSurface->format->palette->colors[idx].r,
                                gSdlSurface->format->palette->colors[idx].g,
                                gSdlSurface->format->palette->colors[idx].b);
                    } else {
                        fprintf(f, "\n");
                    }
                }

                // Also dump a small region around center
                fprintf(f, "\n  Center 8x8 pixel block at (%d,%d):\n", w/2-4, h/2-4);
                for (int dy = 0; dy < 8; dy++) {
                    fprintf(f, "    ");
                    for (int dx = 0; dx < 8; dx++) {
                        int px = w/2 - 4 + dx;
                        int py = h/2 - 4 + dy;
                        if (px >= 0 && px < w && py >= 0 && py < h) {
                            fprintf(f, " %3d", pixels8[py * gSdlSurface->pitch + px]);
                        }
                    }
                    fprintf(f, "\n");
                }
            } else {
                fprintf(f, "  gSdlSurface or pixels is NULL!\n");
            }

            // Dump gSdlTextureSurface (RGB888) sample
            fprintf(f, "\n=== gSdlTextureSurface RGB888 pixels ===\n");
            if (gSdlTextureSurface != NULL && gSdlTextureSurface->pixels != NULL) {
                int tw = gSdlTextureSurface->w;
                int th = gSdlTextureSurface->h;
                fprintf(f, "  texture surface: %dx%d, pitch=%d, bpp=%d\n",
                        tw, th, gSdlTextureSurface->pitch, gSdlTextureSurface->format->BitsPerPixel);
                Uint8* rgb = (Uint8*)gSdlTextureSurface->pixels;
                // Sample same grid positions
                int positions[][2] = {
                    {0,0}, {tw/4,0}, {tw/2,0}, {3*tw/4,0},
                    {0,th/4}, {tw/4,th/4}, {tw/2,th/4}, {3*tw/4,th/4},
                    {0,th/2}, {tw/4,th/2}, {tw/2,th/2}, {3*tw/4,th/2},
                    {0,3*th/4}, {tw/4,3*th/4}, {tw/2,3*th/4}, {3*tw/4,3*th/4},
                };
                int bpp = gSdlTextureSurface->format->BytesPerPixel;
                for (int i = 0; i < 16; i++) {
                    int x = positions[i][0];
                    int y = positions[i][1];
                    Uint8* pixel = &rgb[y * gSdlTextureSurface->pitch + x * bpp];
                    if (bpp == 3) {
                        fprintf(f, "  pixel[%4d,%4d] = RGB(%3d,%3d,%3d)\n", x, y, pixel[0], pixel[1], pixel[2]);
                    } else if (bpp == 4) {
                        fprintf(f, "  pixel[%4d,%4d] = RGBA(%3d,%3d,%3d,%3d)\n", x, y, pixel[0], pixel[1], pixel[2], pixel[3]);
                    } else if (bpp == 2) {
                        Uint16 val = *(Uint16*)pixel;
                        fprintf(f, "  pixel[%4d,%4d] = 0x%04x\n", x, y, val);
                    }
                }
            } else {
                fprintf(f, "  gSdlTextureSurface or pixels is NULL!\n");
            }

            fclose(f);
        }
    }

    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
}

} // namespace fallout
