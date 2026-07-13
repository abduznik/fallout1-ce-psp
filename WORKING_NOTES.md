# Working Notes — Fallout 1 CE PSP Port

Process discipline for how we work on this project. This is NOT a project log
(see STATUS.md for what's built and what's working).

1. **Vision-blind.** I cannot see screenshots, the emulator window, or the
   device. Every visual claim must go through boss via Telegram with an
   explicit wait for confirmation. Never narrate visual success ("it renders
   in color now") from logs, return codes, or hope alone.

2. **Diff before theorising.** When something breaks, diff against the last
   confirmed-working commit first. Three confident-but-wrong hypotheses were
   corrected this way (OpenGL hint, data-files confusion, grayscale palette)
   — each cost a full build-deploy cycle. Diffing finds the actual delta.

3. **Measure before fixing.** Log the actual values (pixel formats, pitches,
   return codes, coordinates) that confirm the root cause BEFORE writing a
   fix. Report those raw values in the same message as the diagnosis. Don't
   skip the confirming test.

4. **Report what you have, not what you infer.** Git status, log lines,
   return codes are real. Plausible fills are not. "I don't have
   confirmation of X yet" is a complete and truthful answer.

5. **"Diagnose only" means stop there.** When told to diagnose without
   fixing, don't propose or skip to a fix. This pattern was corrected
   multiple times and should stay corrected.

6. **Verify the commit landed.** Before declaring a task done, check `git
   status`/`git log` that your changes actually exist on disk. At least one
   write silently failed mid-cycle and wasn't caught until the boss found a
   leftover prompt.
