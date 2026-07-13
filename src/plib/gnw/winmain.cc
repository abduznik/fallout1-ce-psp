#include "plib/gnw/winmain.h"

#include <stdlib.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "game/main.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/svga.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

#ifdef __PSP__
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <pspthreadman.h>
#include <psputility.h>
#include <psputility_msgdialog.h>

// Thread that auto-dismisses the PSP system dialog shown during game init.
// Without this, the game sits forever at sceUtilityMsgDialogUpdate waiting
// for user input — and we can't send input from a terminal session.
int dialogDismissThread(SceSize args, void* argp)
{
    sceKernelDelayThread(3000000);  // Wait 3s for dialog to appear
    sceUtilityMsgDialogAbort();     // Dismiss it
    sceKernelExitDeleteThread(0);
    return 0;
}
#endif

namespace fallout {

// 0x53A290
bool GNW95_isActive = false;

#if _WIN32
// 0x53A294
HANDLE GNW95_mutex = NULL;
#endif

// 0x6B0760
char GNW95_title[256];

int main(int argc, char* argv[])
{
    int rc;

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        return 0;
    }
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

#ifdef __PSP__
    chdir("ms0:/PSP/GAME/FOUT00001/");
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    // Write a startup marker to the debug file so we can confirm 
    // the app boots at all.
    SceUID fd = sceIoOpen("ms0:/psp_debug.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
    if (fd >= 0) {
        const char* msg = "=== FALLOUT1-CE PSP STARTUP ===\n";
        sceIoWrite(fd, msg, strlen(msg));
        sceIoClose(fd);
    }
    // Create a thread to auto-dismiss the PSP system dialog that appears
    // during game init (save-data check). This dialog blocks the render
    // loop and prevents svga_init() from completing.
    SceUID dismissThread = sceKernelCreateThread("dialog_dismiss",
        (SceKernelThreadEntry)dialogDismissThread,
        0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
    if (dismissThread >= 0) {
        sceKernelStartThread(dismissThread, 0, NULL);
    }
#endif

    SDL_ShowCursor(SDL_DISABLE);

    GNW95_isActive = true;
    rc = gnw_main(argc, argv);

#if _WIN32
    CloseHandle(GNW95_mutex);
#endif

    return rc;
}

} // namespace fallout

int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
