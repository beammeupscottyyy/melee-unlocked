/* Tick re-run (experimental, MELEE_TICK_REDO=1, offline matches only; host.h tick_redo_*).
 *
 * A press waits for the next 60 Hz tick before the game sees it: 8.3 ms on average. Here the game
 * keeps the state each tick started from (taken after the frame is drawn, so it costs the picture
 * nothing). When the controller changes while the game waits for the next tick, the host ends the
 * wait, the gate below asks for one more engine body, and that body starts by going back to the kept
 * state and reading the controller again: the same tick, run and drawn a second time with the newer
 * input. Frame counts do not change; only how late in a tick the controller is read.
 *
 * Never online, in a pending online match, in replay playback or under the console rollback. */
#include <dolphin/os.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/video.h>
#include "mu_native.h"

int mu_online_active(void);
int mu_online_pending(void);
int mu_console_rollback_active(void);
int mu_replay_abi_active(void);
int mu_online_abi_command(unsigned int command, const unsigned char* payload, unsigned int size,
                          unsigned char* response, unsigned int capacity, unsigned int* response_size);
void mu_alarms_hold(void);
void mu_alarms_release(void);
void mu_online_audio_match_start(int local_index, int input_source);
void mu_online_audio_frame_begin(void);
void mu_online_audio_counts(unsigned int* started, unsigned int* adopted, unsigned int* stopped);
void fn_800195FC(void);
extern char* getenv(const char* name);

#define CMD_TICK_REDO 0xC7

static int redo_armed;     /* the host keeps a state for this tick */
static int redo_pending;   /* the next engine body is a re-run */

static int redo_command(unsigned char op)
{
    static unsigned char response[4096];   /* the host's minimum response capacity */
    unsigned int got = 0;
    if (mu_online_abi_command(CMD_TICK_REDO, &op, 1, response, sizeof response, &got) != 0 || got < 1) {
        return 0;
    }
    return response[0];
}

static int redo_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char* v = getenv("MELEE_TICK_REDO");
        on = v != NULL && v[0] == '1';
    }
    return on;
}

/* The frame buffer the tick's first picture went to, still waiting for the retrace to be shown: the
 * re-run's picture replaces it there. With two buffers there is no free one before the retrace, and
 * waiting for one would hold the picture back a whole frame (and leave the scene loop waiting inside
 * the copy from then on). -1 when the buffers are in any other state: then there is no re-run. */
static int redo_pending_buffer(void)
{
    int i, found = -1;
    for (i = 0; i < HSD_VI_XFB_MAX; i++) {
        switch (HSD_VIData.xfb[i].status) {
        case HSD_VI_XFB_NEXT:
            if (found >= 0) return -1;
            found = i;
            break;
        case HSD_VI_XFB_DRAWING:
        case HSD_VI_XFB_WAITDONE:
        case HSD_VI_XFB_DRAWDONE:
        case HSD_VI_XFB_COPYEFB:
            return -1;
        default:
            break;
        }
    }
    return found;
}

static int redo_allowed(void)
{
    return !mu_online_active() && !mu_online_pending() && !mu_console_rollback_active() && !mu_replay_abi_active();
}

/* Sounds started in an engine body are kept per frame while this is on (mu_online_audio.c). */
int mu_redo_tracking(void)
{
    return redo_armed && redo_on() && redo_allowed();
}

/* The pad wait of the scene loop (mu_online_engine_gate, offline): nonzero runs one engine body now. */
int mu_redo_gate(void)
{
    if (!redo_on() || !redo_armed || !redo_allowed() || redo_pending_buffer() < 0) {
        return 0;
    }
    if (!redo_command(2)) {
        return 0;
    }
    redo_pending = 1;
    return 1;
}

/* Top of an engine body (mu_lab_frame_begin). */
void mu_redo_frame_begin(void)
{
    int ok;
    if (!redo_pending) {
        if (mu_redo_tracking()) {
            mu_online_audio_frame_begin();
        }
        return;
    }
    redo_pending = 0;
    mu_alarms_hold();   /* timers are hardware: they keep their schedule through a load */
    ok = redo_command(3);
    mu_alarms_release();
    redo_pending = 0;   /* the load brought back the value from before the wait */
    if (ok) {
        /* ... and the armed flag as it was when that state was kept, which is "not yet" for the first
         * state of a stretch of play (it is kept before the flag is set). A tick is only ever run again
         * while armed, and its second run must find the sounds of its first: without this such a tick
         * ran untracked, started its sounds a second time and emptied the sound ring after it. */
        redo_armed = 1;
        static unsigned int runs;
        if (++runs % 50 == 0) {
            unsigned int started, adopted, stopped;
            mu_online_audio_counts(&started, &adopted, &stopped);
            OSReport("[redo] %u ticks run twice; sounds since the ring was emptied: %u started, %u taken over from a first run, %u stopped\n", runs, started, adopted, stopped);
        }
        {
        const int buffer = redo_pending_buffer();   /* video state is outside the kept state */
        if (buffer >= 0) {
            HSD_VIData.xfb[buffer].status = HSD_VI_XFB_DRAWING;
        }
        fn_800195FC();   /* the pad alarm's own work: the controller as it is now, for this body */
        }
    }
    if (mu_redo_tracking()) {
        mu_online_audio_frame_begin();   /* this body adopts the sounds the first run started */
    }
}

/* After a frame is drawn (mu_hud_report): keep this tick's state for a possible re-run of the next. */
void mu_redo_after_render(void)
{
    if (!redo_on()) {
        return;
    }
    if (!redo_allowed()) {
        if (redo_armed) {
            redo_command(0);
            redo_armed = 0;
        }
        return;
    }
    {
        const int was = redo_armed;
        redo_armed = redo_command(1);
        if (redo_armed && !was) {
            mu_online_audio_match_start(0, 0);   /* an empty sound ring for this stretch of play */
        }
    }
}
