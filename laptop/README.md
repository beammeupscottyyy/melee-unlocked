# Laptop low-latency fork

This fork tracks [hero88go/melee-unlocked](https://github.com/hero88go/melee-unlocked) and adds a
tuned setup for an MSI GF75 (GTX 1650 + Intel UHD 630, 120 Hz panel, 32 GB) with a GameCube
adapter. Everything the fork adds lives in `laptop/` and `.github/workflows/sync-upstream.yml`,
which upstream never touches, so the fork stays level with upstream without conflicts.

## Why a profile and not engine changes (yet)

An audit of this laptop's own game logs (0.9.97/0.9.98, Nov 2026) found:

| Finding | Measured | What the profile does |
|---|---|---|
| The GPU is nearly idle | 1.2 to 2.2 ms per frame of an 8.3 ms budget | Spends it on 3x resolution and 16x AF |
| The CPU's draw submission is the cost | 1.2 to 7.1 ms per frame | Nothing here; upstream cut this work in Nov 2026 releases |
| NVIDIA Reflex was off | `reflex 0`, though Streamline loads Reflex on this GPU | On + Boost |
| Windowed with V-Sync | "the display owns the frames" | Exclusive fullscreen, cap kept at 120 |
| Audio queue | 120 ms from game to engine in Auto | Low latency, 20 ms buffer |
| Frame spikes | 58 to 105 ms, one 1572 ms on a scale change | Fixed internal scale; upstream pacing fixes |

Upstream already does the hard input work: the adapter is read every millisecond on a
time-critical thread, early inputs send presses between frames online, and Reflex markers are
placed around the simulation. Engine changes in this fork wait until a lag test on the newest
upstream release shows a specific problem worth carrying a patch for.

## Use

1. Update the game to the newest release (launcher: Update and restart).
2. Close the game. In PowerShell:

   ```powershell
   .\laptop\apply_profile.ps1 -GameDir "C:\path\to\MeleeUnlocked"
   ```

   It backs up `port-settings.ini` first and changes only the keys in the profile.
3. Windows: play plugged in, power mode Best performance.

If you hear crackling, raise `audio_buffer_ms` to 30 (F1 > Audio).

## Measuring

The game has a Lossless adapter lag test (F1 > Controls, or `lagtestnext 1` in the ini for the next
launch only) that sets up the run the way the Lossless Adapter Manager sets up Slippi Dolphin, so
results compare directly. Record before and after any change here.

## Sync

`sync-upstream.yml` merges upstream `main` into this fork's `main` every six hours and can be run
by hand from the Actions tab. GitHub turns Actions off on new forks; enable them once under
Actions.
