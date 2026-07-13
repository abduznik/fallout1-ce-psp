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
