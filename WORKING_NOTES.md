# Working Notes — Fallout 1 CE PSP Port

## How we work on this project

1. **Vision-blind.** I cannot see screenshots, the emulator window, or the
   device. Every visual claim must go through the boss via Telegram with an
   explicit wait for confirmation. Never narrate visual success from logs,
   return codes, or hope.

2. **Diff before deducing.** When something breaks, diff the current code
   against the last confirmed-working commit first. This project's fastest
   fixes came from diffing; the slowest and wrongest came from platform-
   knowledge-only theorising (three wrong hypotheses corrected: OpenGL
   hint, data-files-absent, grayscale-palette).

3. **Measure before fixing.** Log the actual values (formats, pitches,
   return codes, coordinates) that confirm the root cause before writing
   the fix. Report those raw values in the same message as the diagnosis.

4. **Report mechanical facts you have** (git status, log lines, return
   codes). Do not fill gaps with plausible inference. "I don't have
   confirmation of X yet" is a complete and correct answer.

5. **Stop at "diagnose only".** When asked to diagnose without fixing,
   actually stop there. Several sessions skipped the confirming test and
   went straight to a fix — each time corrected, but shouldn't need
   correcting again.

6. **Verify the commit landed.** Before declaring a task done, check
   `git status` / `git log` that the file or commit actually exists. One
   write silently failed mid-task (interrupted command) and wasn't caught
   until the boss noticed a leftover prompt.

7. **One hypothesis at a time.** Prepend diagnostic output with a clear
   label so the boss can read exactly which theory was being tested. Every
   debug build is proving or disproving exactly one thing.

## Build & deploy cycle

1. Commit + push to `github.com/abduznik/fallout1-ce-psp` (branch `main`)
2. SSH to homelab (`arb` host): `git pull origin main`
3. Build: `export PSPDEV=/opt/pspdev && export PATH=$PSPDEV/bin:$PATH && cd build_psp && touch ../src/plib/gnw/svga.cc && make -j4`
4. SCP EBOOT.PBP back: `scp arb:~/fallout1-ce-psp/build_psp/EBOOT.PBP /f/emulators/ppsspp/memstick/PSP/GAME/FOUT00002/EBOOT.PBP`
5. Kill stale PPSSPP: `cmd //c "taskkill /F /IM PPSSPPWindows64.exe"`
6. Launch: `cmd //c "start /B F:\emulators\ppsspp\PPSSPPWindows64.exe F:\emulators\ppsspp\memstick\PSP\GAME\FOUT00002\EBOOT.PBP"`
7. Wait 35–45s for intros, then take screenshot via PowerShell cap2.ps1
8. Read debug logs from `F:\emulators\ppsspp\memstick\psp_debug.txt`

Note: PSP firmware homebrew path `ms0:/PSP/GAME/FOUT00002/` contains the
game data (MASTER.DAT, CRITTER.DAT, DATA/). The EBOOT.PBP gets overwritten
each deploy via SCP from the homelab.
