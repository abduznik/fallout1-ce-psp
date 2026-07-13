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
        // Write test pattern to gSdlTextureSurface (final RGB surface before
        // texture upload). Handle both RGB888 and RGB565/16-bit formats.
        if (gSdlTextureSurface != NULL && gSdlTextureSurface->pixels != NULL) {
            int tw = gSdlTextureSurface->w;
            int th = gSdlTextureSurface->h;
            int pitch = gSdlTextureSurface->pitch;
            int bpp = gSdlTextureSurface->format->BytesPerPixel;
            Uint32 fmt = gSdlTextureSurface->format->format;
            Uint8* rgb = (Uint8*)gSdlTextureSurface->pixels;

            // Helper: write a pixel in native format
            auto writePixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b) {
                if (x < 0 || x >= tw || y < 0 || y >= th) return;
                Uint8* p = &rgb[y * pitch + x * bpp];
                if (bpp == 2) {
                    // RGB565: RRRRRGGGGGGBBBBB
                    Uint16 val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                    *(Uint16*)p = val;
                } else if (bpp == 3) {
                    p[0] = r; p[1] = g; p[2] = b;
                } else if (bpp == 4) {
                    p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
                }
            };

            // Draw a 32x32 rainbow test pattern at top-left
            for (int y = 0; y < 32 && y < th; y++) {
                for (int x = 0; x < 32 && x < tw; x++) {
                    writePixel(x, y, (Uint8)(x * 8), (Uint8)(y * 8), (Uint8)(128 + x * 4 - y * 4));
                }
            }

            // Draw a white border around the entire texture
            for (int x = 0; x < tw; x++) {
                writePixel(x, 0, 255, 255, 255);
                writePixel(x, th-1, 255, 255, 255);
            }
            for (int y = 0; y < th; y++) {
                writePixel(0, y, 255, 255, 255);
                writePixel(tw-1, y, 255, 255, 255);
            }

            // Also draw colored squares showing first 16 palette entries
            if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
                SDL_Palette* pal = gSdlSurface->format->palette;
                for (int i = 0; i < 16 && i < (int)pal->ncolors; i++) {
                    int px = tw - 80 + (i % 8) * 10;
                    int py = 4 + (i / 8) * 10;
                    for (int dy = 0; dy < 8 && py+dy < th; dy++) {
                        for (int dx = 0; dx < 8 && px+dx < tw; dx++) {
                            writePixel(px+dx, py+dy, pal->colors[i].r, pal->colors[i].g, pal->colors[i].b);
                        }
                    }
                }
            }

            // Log texture format info (will appear as pattern on screen)
            // Write the format as colored bars at the bottom
            char fmtStr[64];
            snprintf(fmtStr, sizeof(fmtStr), "FMT:%dx%dbpp%d", tw, th, bpp);
            for (size_t ci = 0; ci < strlen(fmtStr) && ci < 60; ci++) {
                int px = 4 + ci * 8;
                int py = th - 20;
                Uint8 brightness = (Uint8)(fmtStr[ci] * 4);
                for (int dy = 0; dy < 8 && py+dy < th; dy++) {
                    for (int dx = 0; dx < 6 && px+dx < tw; dx++) {
                        writePixel(px+dx, py+dy, brightness, brightness, 255-brightness);
                    }
                }
            }
        }
    }

    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
}

} // namespace fallout
