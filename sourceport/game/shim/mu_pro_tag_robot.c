/* A pro AI's name tag: the small white robot after the name (owner 11-19).
 * Drawn by the game with the tag's own letters (the name tag text's render callback), at the place
 * the tag set this frame, so it is part of the tag: it cannot lag, drift or shake against the name
 * the way a picture drawn over the window from a reported position did. Display only. */
#include "mu_native.h"
#include <dolphin/gx.h>
#include <melee/gm/gmvs.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/sislib.h>

int mu_nametag_position(int slot, float* x, float* y);        /* if/ifnametag.c: the tag, 640x480 units */
int mu_pro_tag_icon(int slot, float* dx, float* width);       /* if/ifnametag.c: the room left for the robot */

#define ROBOT_SIZE 64            /* texels a side */
#define ROBOT_SPAN 12.0f         /* the picture's side in robot units; the robot is 11.2 wide */
#define ROBOT_TOP (-6.325f)      /* the picture's top edge in robot units (the robot is centred in it) */
#define ROBOT_DY (-32.8f)        /* the robot's middle above the tag's point, 480 units: the letters' middle (measured) */
#define ROBOT_HEIGHT 14.5f       /* at most the letters' height, 480 units (ten robot units) */

static unsigned char robot_image[ROBOT_SIZE * ROBOT_SIZE * 4];   /* GX RGBA8: 4x4 tiles, AR then GB */
static int robot_built;

static int in_box(float x, float y, float x0, float y0, float x1, float y1, float r)
{
    float dx, dy;
    if (x < x0 || x > x1 || y < y0 || y > y1) return 0;
    dx = x < x0 + r ? x0 + r - x : x > x1 - r ? x - (x1 - r) : 0.0f;
    dy = y < y0 + r ? y0 + r - y : y > y1 - r ? y - (y1 - r) : 0.0f;
    return dx * dx + dy * dy <= r * r;
}

static int in_disc(float x, float y, float cx, float cy, float r)
{
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

/* 0 nothing, 1 white, 2 dark, at a point in robot units (x right, y down, the head's middle near 0). */
static int robot_at(float x, float y)
{
    int v = 0;
    if (in_disc(x, y, 0.0f, -4.3f, 1.05f)) v = 1;                           /* antenna ball */
    if (in_box(x, y, -0.35f, -3.6f, 0.35f, -2.4f, 0.0f)) v = 1;             /* antenna */
    if (in_box(x, y, -5.6f, -0.9f, -4.0f, 1.9f, 0.6f)) v = 1;               /* ears */
    if (in_box(x, y, 4.0f, -0.9f, 5.6f, 1.9f, 0.6f)) v = 1;
    if (in_box(x, y, -4.4f, -2.9f, 4.4f, 4.7f, 1.6f)) v = 1;                /* head */
    if (in_disc(x, y, -1.75f, 0.2f, 1.2f) || in_disc(x, y, 1.75f, 0.2f, 1.2f)) v = 2;   /* eyes */
    if (in_box(x, y, -2.3f, 2.3f, 2.3f, 3.6f, 0.5f)) v = 2;                 /* mouth */
    if (in_box(x, y, -0.95f, 2.5f, -0.55f, 3.4f, 0.0f) || in_box(x, y, 0.55f, 2.5f, 0.95f, 3.4f, 0.0f)) v = 1;
    return v;
}

static void robot_build(void)
{
    int px, py, sx, sy;
    for (py = 0; py < ROBOT_SIZE; py++) {
        for (px = 0; px < ROBOT_SIZE; px++) {
            int white = 0, dark = 0;
            unsigned char* tile = robot_image + ((py / 4) * (ROBOT_SIZE / 4) + px / 4) * 64;
            const int at = (py % 4) * 4 + px % 4;
            unsigned r = 255, g = 255, b = 255;
            for (sy = 0; sy < 4; sy++) {
                for (sx = 0; sx < 4; sx++) {
                    const float x = ((float) px + ((float) sx + 0.5f) / 4.0f) / ROBOT_SIZE * ROBOT_SPAN - ROBOT_SPAN / 2;
                    const float y = ((float) py + ((float) sy + 0.5f) / 4.0f) / ROBOT_SIZE * ROBOT_SPAN + ROBOT_TOP;
                    const int v = robot_at(x, y);
                    if (v == 1) white++;
                    else if (v == 2) dark++;
                }
            }
            if (white + dark > 0) {   /* the covered part's colour; the edge's cover is the alpha */
                r = (unsigned) (255 * white + 24 * dark) / (unsigned) (white + dark);
                g = (unsigned) (255 * white + 24 * dark) / (unsigned) (white + dark);
                b = (unsigned) (255 * white + 34 * dark) / (unsigned) (white + dark);
            }
            tile[at * 2] = (unsigned char) ((white + dark) * 255 / 16);
            tile[at * 2 + 1] = (unsigned char) r;
            tile[32 + at * 2] = (unsigned char) g;
            tile[32 + at * 2 + 1] = (unsigned char) b;
        }
    }
    robot_built = 1;
}

/* The name tag text's render callback: SIS has set its camera, vertex layout (position and one
 * texture coordinate, direct) and blending, and sets the letters' own state again after this. */
void mu_pro_tag_robot_draw(void* object)
{
    HSD_GObj* gobj = object;
    HSD_Text* text = gobj != NULL ? gobj->user_data : NULL;
    static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    const GXColor white = { 255, 255, 255, 255 };
    int slot, i, state_set = 0;
    if (text == NULL) return;
    for (slot = 0; slot < 4; slot++) {
        float x = 0.0f, y = 0.0f, dx = 0.0f, width = 0.0f, u, half, cx, cy;
        float ws;
        if (Player_GetPlayerSlotType(slot) == Gm_PKind_NA) continue;
        if (!mu_nametag_position(slot, &x, &y) || !mu_pro_tag_icon(slot, &dx, &width) || width <= 0.0f) continue;
        /* The name sits half the robot's room left of the tag's point; the robot's middle is half the
         * name plus half its room right of the name's middle: the tag's point plus half the name. */
        /* True widescreen stretches this canvas sideways; the tag narrows its letters by ws to stay in
         * shape (NAMETAG_TEXT_SCALE_X), and so does the robot. */
        ws = mu_widescreen() ? 0.26952344f / 0.4f : 1.0f;
        u = ROBOT_HEIGHT / 10.0f;
        if (width / ws * 0.98f / 11.2f < u) u = width / ws * 0.98f / 11.2f;
        if (u < 0.05f) continue;
        half = u * ROBOT_SPAN / 2;
        cx = x + dx;
        cy = y + ROBOT_DY + (ROBOT_TOP + ROBOT_SPAN / 2) * u;   /* the picture's middle; the head's stays on the line */
        if (!state_set) {
            GXTexObj tex;
            if (!robot_built) robot_build();
            GXInitTexObj(&tex, (void*) robot_image, ROBOT_SIZE, ROBOT_SIZE, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
            GXInitTexObjLOD(&tex, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
            GXLoadTexObj(&tex, GX_TEXMAP0);
            GXSetTevColor(GX_TEVREG0, white);
            GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
            GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);
            GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
            GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
            state_set = 1;
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        for (i = 0; i < 4; i++) {
            GXPosition3f32(cx + (uv[i][0] * 2 - 1) * half * ws, -(cy + (uv[i][1] * 2 - 1) * half), text->pos_z);
            GXTexCoord2f32(uv[i][0], uv[i][1]);
        }
        GXEnd();
    }
}

/* Snapshot exclusions: the baked picture is not game state. */
MU_EXCLUSIONS(pro_tag_robot,
              MU_EXCLUDE(robot_image),
              MU_EXCLUDE(robot_built))
