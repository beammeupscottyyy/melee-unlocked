/* Replace only custom Online label textures, preserving native row slots and highlight. Shared disc descriptors
 * and animation image tables (including Update) always remain untouched. */
#include <mu_native.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/types.h>
#include <melee/sc/types.h>
#include <sysdolphin/baselib/dobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>
#include <sysdolphin/baselib/mobj.h>
#include <sysdolphin/baselib/sislib.h>
#include <string.h>
#include <dolphin/os.h>
#include <melee/lb/lbspdisplay.h>
#include <sysdolphin/baselib/gobjuserdata.h>
#include <sysdolphin/baselib/memory.h>
#include <sysdolphin/baselib/pobj.h>
#include <sysdolphin/baselib/tobj.h>
#include "mu_menu_label.h"
#include "mu_online_pages.h"
#include "mu_online_label_art.h"
#include "mu_online_label_art_fix.h"
#include "mu_test_ui.h"
#include "mu_menu_heading_art.h"
#if MU_SQUAD
/* Based on Squad Strike by Chris (x9x9x9x9x9), https://github.com/x9x9x9x9x9/squad-strike v1.0.0, used with the author's permission (11-03-2026) */
#include "mu_squad_label_art.h"
#else
#line 21 /* OFF diagnostics retain the original DISC_SET line numbers. */
#endif

static int pages_label_swap(HSD_JObj* label, int menu_kind, int selection, int capture);
void mu_slippi_friends_label(HSD_JObj* label, int menu_kind, int selection, int capture)
{
    if (pages_label_swap(label, menu_kind, selection, capture)) return;   /* the two pages: all rows */
    static HSD_ImageDesc* native[8];
#ifdef MELEE_DOUBLES
    static HSD_ImageDesc image[4];
#else
    static HSD_ImageDesc image[3];
#endif
#if MU_SQUAD
    /* Based on Squad Strike by Chris (x9x9x9x9x9), https://github.com/x9x9x9x9x9/squad-strike v1.0.0, used with the author's permission (11-03-2026) */
    static HSD_ImageDesc squad_image;
#else
#line 30
#endif
    int kind = mu_slippi_menu_label_kind(selection), slot;
    HSD_DObj* dobj;
    if (!mu_slippi_menus_enabled() || !mu_slippi_yougame_menu() ||
        menu_kind != MENU_KIND_8 || selection < 0 || selection >= 8) return;
    if (capture) {
        if (selection == 0) for (slot = 0; slot < 8; ++slot) native[slot] = NULL;
        native[selection] = NULL;
    }
    if (label == NULL) return;
    for (dobj = label->u.dobj; dobj != NULL; dobj = dobj->next) {
        HSD_TObj* texture;
        if (dobj->mobj == NULL) continue;
        for (texture = dobj->mobj->tobj; texture != NULL; texture = texture->next) {
            HSD_ImageDesc* original = texture->imagedesc;
            if (original != NULL && original->width == MU_LABEL_W &&
                original->height == MU_LABEL_H && original->format == GX_TF_IA4) {
                HSD_ImageDesc* replacement = NULL;
                /* All eight ORIGINAL descriptors are captured before any visible
                 * row is swapped, including Party (slot 4), which is hidden at
                 * the top level. Descriptors and image tables stay immutable. */
                if (capture) { native[selection] = original; continue; }
                if (kind == 1 || kind == 2) {
                    image[kind - 1] = *original;
                    DISC_SET(image[kind - 1].image_ptr,
                             kind == 1 ? mu_baked_friends : mu_baked_slippi);
                    replacement = &image[kind - 1];
#ifdef MELEE_DOUBLES
                } else if (kind == 9) {
                    image[3] = *original;
                    DISC_SET(image[3].image_ptr, mu_baked_doubles);
                    replacement = &image[3];
#endif
                } else if (kind == 8) {
                    image[2] = *original;
                    DISC_SET(image[2].image_ptr, mu_baked_tournament);
                    replacement = &image[2];
#if MU_SQUAD
                /* Based on Squad Strike by Chris (x9x9x9x9x9), https://github.com/x9x9x9x9x9/squad-strike v1.0.0, used with the author's permission (11-03-2026) */
                } else if (kind == 10) {
                    squad_image = *original;
                    DISC_SET(squad_image.image_ptr, mu_baked_squad);
                    replacement = &squad_image;
#else
#line 66
#endif
                } else if (kind >= 3 && kind <= 5) {
                    replacement = native[2 + selection];
                } else if ((kind == 6 || kind == 7) &&
                           original == native[2 + selection]) {
                    /* Restore if the panel reuses a submenu row on Back.
                     * A newly created Ranked/Unranked row is never swapped. */
                    replacement = native[selection];
                }
                /* Ranked/Unranked (kinds 6/7) keep their native art, NO swap. */
                if (replacement && replacement->width == original->width &&
                    replacement->height == original->height && replacement->format == original->format)
                    texture->imagedesc = replacement;
            }
        }
    }
}

/* Use exactly the native cursor creation/animation path to capture all eight
 * Online slots, without touching texture animation internals. The image
 * descriptors belong to the loaded menu archive, not these temporary JObjs.
 * Run at menu creation, before the first custom row swap; recapture on return
 * so a reloaded archive never leaves stale pointers. */
void mu_slippi_menu_labels_init(StaticModelSyms* model, int menu_kind)
{
    int slot, track;
    if (!mu_slippi_menus_enabled() || !mu_slippi_yougame_menu() || menu_kind != MENU_KIND_8 || !model) return;
    mu_slippi_friends_label(NULL, menu_kind, 0, 1); /* clear an earlier archive, even if allocation fails */
    for (slot = 0; slot < 8; ++slot) {
        HSD_JObj* parts[7];
        HSD_JObj* cursor = HSD_JObjLoadJoint(model->joint);
        if (!cursor) return;
        HSD_JObjAddAnimAll(cursor, model->animjoint, model->matanim_joint, model->shapeanim_joint);
        /* These are the same cursor-parts indices used by mn_8022B3A0. */
        lb_80011E24(cursor, &parts[1], 3, -1);
        HSD_JObjReqAnimAll(parts[1], 0);
        HSD_JObjAnimAll(parts[1]);
        HSD_JObjReqAnim(parts[1], 140 + 2 * slot);
        for (track = 0xC; track <= 0x13; ++track) mn_8022F3D8(parts[1], track, TOBJ_MASK);
        HSD_JObjAnim(parts[1]);
        mu_slippi_friends_label(parts[1], menu_kind, slot, 1);
        HSD_JObjRemoveAll(cursor);
    }
}

/* Keep the game's text object, camera, position, box and font parameters.
 * Replace only its words, after copying the actual catalog stream's prefix.
 * The normal HSD_Text destructor owns both allocations, including notices
 * repeated while the user remains on the same row. */
void mu_slippi_menu_description(HSD_Text* text, const char* message)
{
    unsigned char encoded[1024], stream[128 + sizeof encoded];
    SisBuffer* alloc;
    int prefix, n, i, length;
    if (!text || !message || strlen(message) > 100) return;
    prefix = mu_menu_description_prefix(text->sis_buffer);
    if (!prefix) return;
    memcpy(stream, text->sis_buffer, prefix);
    length = HSD_SisLib_803A67EC(encoded, (u8*) message);
    n = prefix;
    /* The editable ASCII encoder inserts its own 0x0a/0x0b spacing overrides.
     * Keep glyphs/spaces only: the row's SIS prefix supplies the kerning. */
    for (i = 0; i < length;) {
        unsigned op = encoded[i];
        int size = op >= 0x20 ? 2 : mu_menu_sis_command_size(op);
        if (op >= 0x20 || op == 0x1a) {
            memcpy(stream + n, encoded + i, size);
            n += size;
        }
        i += size;
    }
    stream[n] = 0;
    alloc = (SisBuffer*) HSD_SisLib_Alloc(sizeof *alloc);
    if (!alloc) return;
    alloc->data = (u8*) HSD_SisLib_Alloc(n + 1);
    if (!alloc->data) { HSD_SisLib_Free(alloc); return; }
    memcpy(alloc->data, stream, n + 1);
    alloc->end = alloc->data + n;
    alloc->size = n + 1;
    alloc->count = 0;
    if (text->alloc_data) {
        HSD_SisLib_Free(text->alloc_data->data);
        HSD_SisLib_Free(text->alloc_data);
    }
    text->alloc_data = alloc;
    text->sis_buffer = alloc->data;
    text->x60 = NULL;
}

void mu_slippi_tournament_notice(HSD_GObj* menu, const char* message)
{
    MainMenuData* data = menu ? menu->user_data : NULL;
    if (data) mu_slippi_menu_description(data->description, message);
}

void mu_slippi_signin_notice(HSD_GObj* menu)
{
    MainMenuData* data;
    u8 query = 2;
    u8 link_line[128] = { 0 };
    unsigned int got = 0;
    if (!menu || !mu_slippi_menus_enabled() || !mu_slippi_yougame_menu()) return;
    data = menu->user_data;
    if (!data) return;
    if (mu_slippi_cmd(0xE7, &query, 1, link_line, sizeof link_line, &got) == 0 &&
        got == sizeof link_line && link_line[0]) {
        link_line[sizeof link_line - 1] = 0;
        mu_slippi_menu_description(data->description, (const char*) link_line);
    } else {
        mu_slippi_menu_description(data->description,
                                  "Sign in to Slippi to play Direct, Teams and Party");
    }
}


/* The panel owns this state, including the replacement title image. The retail
 * four-byte panel data stays first so its animation proc keeps its ABI. */
typedef struct MuSlippiTitlePanel {
    MainMenuPanelData panel;
    HSD_DObj* native_title;
    HSD_JObj* title_joint;
    HSD_TObj* title_texture;
    HSD_ImageDesc* original;
    HSD_ImageDesc image;
    void* allocation;
    HSD_Text* text;
    const char* fallback_reason;
    u32 was_hidden;
    int logged;
    int title_id;   /* heading word shown: 0 native, 1 YouGame, 2 Slippi */
    int refused;    /* title quad is not Online Play's 168x28 I4: keep the native heading */
} MuSlippiTitlePanel;

typedef struct MuTitleBounds {
    float left, right, bottom, top, z;
} MuTitleBounds;

/* Read the title quad's indexed positions, not a screen-space approximation.
 * Menu archives use unskinned indexed XYZ vertices. Unsupported geometry
 * leaves the native title visible rather than hiding an unrelated object. */
static int mu_slippi_title_bounds(HSD_JObj* joint, HSD_DObj* dobj, MuTitleBounds* b)
{
    HSD_PObj* p = dobj->pobj;
    HSD_VtxDescList* v;
    HSD_VtxDescList* positions = NULL;
    int stride = 0, offset = 0, vertices, i, k;
    const u8* cursor;
    if (!p || p->next || !p->display || !p->verts ||
        (p->flags & (3 << 12))) return 0; /* no envelopes / shape sets */
    for (v = p->verts; v->attr != GX_VA_NULL; ++v) {
        int bytes;
        if (v->attr_type == GX_DIRECT && v->attr <= GX_VA_TEX7MTXIDX) bytes = 1;
        else if (v->attr_type == GX_INDEX8) bytes = 1;
        else if (v->attr_type == GX_INDEX16) bytes = 2;
        else return 0;
        if (v->attr == GX_VA_POS) { positions = v; offset = stride; }
        stride += bytes;
    }
    if (!positions || (positions->attr_type != GX_INDEX8 && positions->attr_type != GX_INDEX16) ||
        positions->comp_type != GX_F32 ||
        positions->comp_cnt != GX_POS_XYZ || DISC_NULL(positions->vertex)) return 0;
    cursor = p->display;
    if ((*cursor & 0xf8) != GX_QUADS && (*cursor & 0xf8) != GX_TRIANGLESTRIP &&
        (*cursor & 0xf8) != GX_TRIANGLEFAN) return 0;
    vertices = (cursor[1] << 8) | cursor[2];
    if (vertices != 4 || 3 + vertices * stride > p->n_display * 32) return 0;
    HSD_JObjSetupMatrix(joint);
    cursor += 3;
    for (i = 0; i < vertices; ++i, cursor += stride) {
        unsigned index = cursor[offset];
        const u8* raw;
        float local[3], world[3];
        if (positions->attr_type == GX_INDEX16) index = (index << 8) | cursor[offset + 1];
        raw = (const u8*) DP(positions->vertex) + index * positions->stride;
        for (k = 0; k < 3; ++k) {
            u32 bits = (u32) raw[k*4] << 24 | (u32) raw[k*4+1] << 16 |
                       (u32) raw[k*4+2] << 8 | raw[k*4+3];
            memcpy(&local[k], &bits, sizeof bits);
        }
        for (k = 0; k < 3; ++k)
            world[k] = joint->mtx[k][0]*local[0] + joint->mtx[k][1]*local[1] +
                       joint->mtx[k][2]*local[2] + joint->mtx[k][3];
        if (i == 0) {
            b->left = b->right = world[0]; b->bottom = b->top = world[1]; b->z = world[2];
        } else {
            if (world[0] < b->left) b->left = world[0];
            if (world[0] > b->right) b->right = world[0];
            if (world[1] < b->bottom) b->bottom = world[1];
            if (world[1] > b->top) b->top = world[1];
        }
    }
    return b->top > b->bottom && b->right - b->left > 3 * (b->top - b->bottom);
}

/* Node 0x29 is the panel's animated heading group (fn_80229BF4).
 * Its rightmost monochrome text quad is the current title; the breadcrumb
 * (1-P Mode) is to its left. Never traverse its siblings (the panel art). */
static void mu_slippi_title_find(HSD_JObj* joint, MuSlippiTitlePanel* state, MuTitleBounds* best)
{
    HSD_DObj* d;
    HSD_JObj* child;
    if (!joint) return;
    if (!(joint->flags & (JOBJ_HIDDEN | JOBJ_PTCL | JOBJ_SPLINE))) {
        for (d = joint->u.dobj; d; d = d->next) {
            HSD_TObj* texture = d->mobj ? d->mobj->tobj : NULL;
            HSD_ImageDesc* image = texture ? texture->imagedesc : NULL;
            MuTitleBounds bounds;
            if (!image || (d->flags & DOBJ_HIDDEN) ||
                (image->format != GX_TF_I4 && image->format != GX_TF_IA4) ||
                !mu_slippi_title_bounds(joint, d, &bounds)) continue;
            if (!state->native_title || bounds.left > best->left) {
                state->native_title = d; state->title_joint = joint; *best = bounds;
            }
        }
    }
    for (child = joint->child; child; child = child->next)
        mu_slippi_title_find(child, state, best);
}

static void mu_slippi_title_close(MuSlippiTitlePanel* state)
{
    if (state->text) {
        HSD_SisLib_803A5CC4(state->text);
        state->text = NULL;
        state->native_title->flags = (state->native_title->flags & ~DOBJ_HIDDEN) | state->was_hidden;
    }
    if (state->title_texture && state->title_texture->imagedesc == &state->image)
        state->title_texture->imagedesc = state->original;
    if (state->allocation) HSD_Free(state->allocation);
    state->allocation = NULL;
    state->title_texture = NULL;
    state->original = NULL;
    state->native_title = NULL;
    state->title_joint = NULL;
}

static void mu_slippi_title_panel_free(void* data)
{
    MuSlippiTitlePanel* state = (MuSlippiTitlePanel*) data;
    mu_slippi_title_close(state);
    HSD_Free(state);
}

void mu_slippi_title_panel_init(HSD_GObj* panel)
{
    MuSlippiTitlePanel* state;
    /* The panel is made before the bridge reports its layout; the update decides per frame. */
    if (!mu_slippi_menus_enabled()) return;
    state = (MuSlippiTitlePanel*) HSD_MemAlloc(sizeof *state);
    if (!state) return;
    memset(state, 0, sizeof *state);
    state->panel = *(MainMenuPanelData*) panel->user_data;
    GObj_RemoveUserData(panel);
    GObj_InitUserData(panel, 0, mu_slippi_title_panel_free, state);
}

/* The heading words are baked at the native title's own size and format (168x28 I4) from the
 * pixels of Slippi's Online Play heading (mu_menu_heading_art.h), so they are swapped in texel for
 * texel: same face, slant, weight, size and left edge; the quad's material keeps its colour and
 * animation. Any other title texture keeps the native heading rather than resampling. */
static int mu_slippi_title_image(MuSlippiTitlePanel* state)
{
    HSD_ImageDesc* original = state->title_texture->imagedesc;
    if (!state->logged) {
        OSReport("online menu: title imagedesc width=%u height=%u format=%u\n",
                 (unsigned) original->width, (unsigned) original->height, (unsigned) original->format);
        state->logged = 1;
    }
    if (original->width != MU_HEADING_W || original->height != MU_HEADING_H ||
        original->format != GX_TF_I4) return 0;
    state->original = original;
    state->image = *original;
    DISC_SET(state->image.image_ptr, (void*) (state->title_id == 2 ? mu_heading_slippi : mu_heading_yougame));
    OSReport("online menu: heading %s\n", state->title_id == 2 ? "Slippi" : "YouGame");
    return 1;
}

void mu_slippi_title_panel_update(HSD_GObj* panel, int canvas)
{
    MuSlippiTitlePanel* state;
    MuTitleBounds bounds;
    HSD_JObj* heading = NULL;
    int wanted;
    (void) canvas;
    if (panel->user_data_remove_func != mu_slippi_title_panel_free) return;
    state = (MuSlippiTitlePanel*) panel->user_data;
    wanted = mn_804A04F0.cur_menu == MENU_KIND_8 ? mu_slippi_title_active() : 0;
    if (wanted != state->title_id) { mu_slippi_title_close(state); state->title_id = wanted; state->refused = 0; }
    if (!wanted) return;
    if (!state->original) {
        /* Find the title quad only once the heading has finished sliding in (panel state 2): while
         * 1-P Mode's heading moves aside, the rightmost quad can still be the breadcrumb. */
        if (state->refused || state->panel.state != 2 || state->panel.cur_menu != MENU_KIND_8) return;
        lb_80011E24((HSD_JObj*) panel->hsd_obj, &heading, 0x29, -1);
        mu_slippi_title_find(heading, state, &bounds);
        if (!state->native_title) return;
        state->title_texture = state->native_title->mobj->tobj;
        if (!mu_slippi_title_image(state)) {
            OSReport("online menu: heading kept native (title texture is not 168x28 I4)\n");
            mu_slippi_title_close(state);
            state->refused = 1;
            return;
        }
    }
    /* The panel animation may reselect its native image every tick. */
    state->title_texture->imagedesc = &state->image;
}

#if MU_SQUAD
/* Based on Squad Strike by Chris (x9x9x9x9x9), https://github.com/x9x9x9x9x9/squad-strike v1.0.0, used with the author's permission (11-03-2026) */
/* Reuse the Online row's baked label inside the Friends mode toggle. SIS owns the camera. */
void mu_squad_lobby_label_draw(void* object)
{
    GXTexObj tex;
    GXColor white = { 255, 255, 255, 255 };
    extern bool gmSquad_LobbyChosen(void);
    (void) object;
    if (!mu_lobby_css() || !gmSquad_LobbyChosen() || mu_lobby_curtain()) return;
    GXSetTevColor(GX_TEVREG0, white);
    GXInitTexObj(&tex, (void*) mu_baked_squad, MU_LABEL_W, MU_LABEL_H,
                 GX_TF_IA4, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&tex, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(-42.5f, 26.5f, 0); GXTexCoord2f32(0, 0);
    GXPosition3f32(-26.0f, 26.5f, 0); GXTexCoord2f32(1, 0);
    GXPosition3f32(-26.0f, 23.6875f, 0); GXTexCoord2f32(1, 1);
    GXPosition3f32(-42.5f, 23.6875f, 0); GXTexCoord2f32(0, 1);
    GXEnd();
}
#endif

/* ---- The online menu's two pages (mu_online_pages.inc) ----
 * Every row's label is a word set in Slippi's own label style (mu_online_label_art.h, from the
 * Slippi-matched setter in E:/mu-yg/labels): "YouGame Ranked" .. "Slippi Party", "Both Unranked".
 * Squad Strike's is wider than the others (below). Under the launcher the host rewrites the archive's
 * online labels (online_rows.cpp), so no row uses the archive's own image. */
static const unsigned char* pages_art(int id)
{
    /* UI audit previews (mu_test_ui.h). B1: one text height per page (YouGame's 65%, Slippi's 83%; YouGame
     * Unranked cannot be set larger than 65%). B2: Unranked's three slider values at one height. */
    if (mu_ui_fix("B1")) {
        if (!mu_online_pages_page()) {
            switch (id) {
            case MP_L_YG_RANKED: return mu_fx65_yougame_ranked;
            case MP_L_YG_FRIENDS: return mu_fx65_yougame_friends;
            case MP_L_YG_DOUBLES: return mu_fx65_yougame_doubles;
            case MP_L_BOTH_UNRANKED: return mu_fx65_both_unranked;
            case MP_L_SLP_UNRANKED: return mu_fx65_slippi_unranked;
            default: break;
            }
        } else {
            switch (id) {
            case MP_L_SLP_DIRECT: return mu_fx83_slippi_direct;
            case MP_L_SLP_TEAMS: return mu_fx83_slippi_teams;
            case MP_L_SLP_PARTY: return mu_fx83_slippi_party;
            case MP_L_BOTH_UNRANKED: return mu_fx83_both_unranked;
            default: break;
            }
        }
    } else if (mu_ui_fix("B1U")) {
        switch (id) {
        case MP_L_YG_RANKED: return mu_fx65_yougame_ranked;
        case MP_L_YG_FRIENDS: return mu_fx65_yougame_friends;
        case MP_L_YG_DOUBLES: return mu_fx65_yougame_doubles;
        case MP_L_BOTH_UNRANKED: return mu_fx65_both_unranked;
        case MP_L_SLP_UNRANKED: return mu_fx65_slippi_unranked;
        case MP_L_SLP_DIRECT: return mu_fx65_slippi_direct;
        case MP_L_SLP_TEAMS: return mu_fx65_slippi_teams;
        case MP_L_SLP_PARTY: return mu_fx65_slippi_party;
        default: break;
        }
    }
    switch (id) {
    case MP_L_YG_RANKED: return mu_sst_yougame_ranked;
    /* Unranked's three slider values are one row (owner 11-19): one letter height, the largest at which
     * "YouGame Unranked" fits a label widened to MU_SST_UNRANKED_W texels (72%), on one baseline, so the
     * words keep their size and line as the slider moves. Slippi's page sets its other rows at that same
     * height, so every row of both pages reads alike (YouGame's other rows are 71-73%; Squad Strike 65%,
     * its own wide label). The fitted sets were 65%, 83%, 87% and Slippi's rows at full size. */
    case MP_L_YG_UNRANKED: return mu_sst_un_yougame;
    case MP_L_YG_FRIENDS: return mu_sst_yougame_friends;
    case MP_L_YG_DOUBLES: return mu_sst_yougame_doubles;
    case MP_L_YG_DOUBLES_RANKED: return mu_sst_dbl_ranked;   /* the Doubles slider's ranked value, as wide as Unranked's */
    case MP_L_YG_SQUAD: return mu_sst_yougame_squad;
    case MP_L_SLP_UNRANKED: return mu_sst_un_slippi;
    case MP_L_SLP_DIRECT: return mu_sst_page_slippi_direct;
    case MP_L_SLP_TEAMS: return mu_sst_page_slippi_teams;
    case MP_L_SLP_PARTY: return mu_sst_page_slippi_party;
    case MP_L_BOTH_UNRANKED: return mu_sst_un_both;
    case MP_L_TOURNAMENT: return mu_baked_tournament;
    default: return NULL;
    }
}

/* Squad Strike's label (owner's pick 11-18) is MU_SST_SQUAD_W texels wide and centred
 * MU_SST_SQUAD_SHIFT texels right of the native label, between the left cap and the ring. Its label
 * quad is widened and moved to match, inside the plate's black interior; the plate and glow keep their
 * size and place. The three share one position array, so that row gets its own copy (per label id) and
 * the archive and the other rows stay as they are. */
#define ROW_TEXEL (4.69677734375f * 2.0f / 176.0f)   /* the label quad's units per texel */
enum { ROW_PARTS = 3, ROW_VERTS = 128, ROW_ATTRS = 8 };
typedef struct RowGeo {
    u8 pos[ROW_VERTS * 6] __attribute__((aligned(32)));
    HSD_VtxDescList list[ROW_PARTS][ROW_ATTRS];
    HSD_VtxDescList* orig[ROW_PARTS];
    const void* src;
    int width, shift;
    float lift;
} RowGeo;
static RowGeo row_geo[MP_L_END];

static HSD_VtxDescList* row_geo_original(HSD_VtxDescList* verts, int part)
{
    int k;
    for (k = 0; k < MP_L_END; ++k)
        if (verts == row_geo[k].list[part]) return row_geo[k].orig[part];
    return verts;
}
static int row_attr_size(const HSD_VtxDescList* v)
{
    if (v->attr_type == GX_INDEX8) return 1;
    if (v->attr_type == GX_INDEX16) return 2;
    if (v->attr_type != GX_DIRECT) return 0;
    if (v->attr <= GX_VA_TEX7MTXIDX) return 1;
    if (v->attr == GX_VA_CLR0 || v->attr == GX_VA_CLR1) {
        switch (v->comp_type) {
        case GX_RGB565: case GX_RGBA4: return 2;
        case GX_RGB8: case GX_RGBA6: return 3;
        default: return 4;
        }
    }
    return 0;
}
/* Mark which position indices this pobj's display list uses (owner[i] = part + 1). */
static int row_geo_scan(const HSD_PObj* p, const HSD_VtxDescList* list, u8* owner, int part, int* count)
{
    const HSD_VtxDescList* v;
    int stride = 0, offset = -1, wide = 0, size;
    const u8 *cursor, *end;
    for (v = list; v->attr != GX_VA_NULL; ++v) {
        if (!(size = row_attr_size(v))) return 0;
        if (v->attr == GX_VA_POS) { offset = stride; wide = size == 2; }
        stride += size;
    }
    if (offset < 0 || !p->display) return 0;
    cursor = p->display;
    end = cursor + p->n_display * 32;
    while (cursor + 3 <= end && (*cursor & 0xf8) != 0) {
        int n = (cursor[1] << 8) | cursor[2], i;
        cursor += 3;
        if (cursor + n * stride > end) return 0;
        for (i = 0; i < n; ++i, cursor += stride) {
            int index = cursor[offset];
            if (wide) index = (index << 8) | cursor[offset + 1];
            if (index >= ROW_VERTS) return 0;
            if (owner[index] && owner[index] != part + 1) return 0;
            owner[index] = (u8) (part + 1);
            if (index + 1 > *count) *count = index + 1;
        }
    }
    return 1;
}
/* width: the label texture's texels; shift: its centre's texels right of native. A native label
 * (MU_LABEL_W, 0) puts back the archive's positions if this row had ours. */
static void row_geo_apply(HSD_JObj* label, int id, int width, int shift, float lift)
{
    HSD_JObj *group, *kids[5], *j, *parts[ROW_PARTS];
    HSD_PObj* p[ROW_PARTS];
    HSD_VtxDescList* orig[ROW_PARTS];
    const HSD_VtxDescList* pos = NULL;
    RowGeo* g;
    float d, v[ROW_VERTS][3], big = 0.0f;
    u8 owner[ROW_VERTS];
    int n = 0, i, k, count = 0, frac;
    if (!label || !(group = label->parent)) return;
    for (j = group->child; j && n < 5; j = j->next) kids[n++] = j;
    if (n < 5 || kids[1] != label) return;
    parts[0] = kids[0]; parts[1] = kids[1]; parts[2] = kids[3];   /* plate, label, glow */
    for (i = 0; i < ROW_PARTS; ++i) {
        HSD_DObj* dobj = parts[i]->u.dobj;
        if ((parts[i]->flags & (JOBJ_PTCL | JOBJ_SPLINE)) || !dobj || dobj->next || !dobj->pobj || dobj->pobj->next ||
            (dobj->pobj->flags & (3 << 12)) || !dobj->pobj->verts) return;
        p[i] = dobj->pobj;
        orig[i] = row_geo_original(p[i]->verts, i);
    }
    /* The right-hand ring (joint 4) and particle point (joint 10's child) at 5.95 and 7.1, as in the look
     * approved 11-18 (the archive has 5.94998 and 7.09993: about 100 pixels differ by 1 or 2 levels). */
    HSD_JObjSetTranslateX(kids[2], 5.95f);
    if (kids[4]->child) HSD_JObjSetTranslateX(kids[4]->child, 7.1f);
    if (shift == 0 && width == MU_LABEL_W && lift == 0.0f) {
        for (i = 0; i < ROW_PARTS; ++i) p[i]->verts = orig[i];
        return;
    }
    d = (float) shift * ROW_TEXEL;
    g = &row_geo[id];
    memset(owner, 0, sizeof owner);
    for (i = 0; i < ROW_PARTS; ++i) {
        const HSD_VtxDescList* a;
        for (a = orig[i]; a->attr != GX_VA_NULL; ++a)
            if (a->attr == GX_VA_POS) break;
        if (a->attr != GX_VA_POS || a->comp_type != GX_S16 || a->comp_cnt != GX_POS_XYZ || a->stride != 6 ||
            DISC_NULL(a->vertex) || (pos && DP(pos->vertex) != DP(a->vertex))) return;
        pos = a;
        if (!row_geo_scan(p[i], orig[i], owner, i, &count)) return;
    }
    if (g->src != DP(pos->vertex) || g->width != width || g->shift != shift || g->lift != lift) {
        const u8* raw = (const u8*) DP(pos->vertex);
        const float unit = 1.0f / (float) (1 << pos->frac);
        for (k = 0; k < count; ++k) {
            for (i = 0; i < 3; ++i) v[k][i] = (float) (s16) ((raw[k * 6 + i * 2] << 8) | raw[k * 6 + i * 2 + 1]) * unit;
            if (owner[k] == 2) {   /* label quad */
                v[k][0] = v[k][0] * (float) width / (float) MU_LABEL_W + d;
                v[k][1] += lift * ROW_TEXEL;
            }
            for (i = 0; i < 3; ++i) if (v[k][i] > big) big = v[k][i]; else if (-v[k][i] > big) big = -v[k][i];
        }
        /* The wider quad may need a smaller fraction; plate and glow are written again at it. */
        for (frac = pos->frac; frac > 0 && big * (float) (1 << frac) > 32767.0f; --frac) {}
        for (k = 0; k < count; ++k)
            for (i = 0; i < 3; ++i) {
                float f = v[k][i] * (float) (1 << frac);
                s16 q = (s16) (f < 0.0f ? f - 0.5f : f + 0.5f);
                g->pos[k * 6 + i * 2] = (u8) ((u16) q >> 8);
                g->pos[k * 6 + i * 2 + 1] = (u8) q;
            }
        for (i = 0; i < ROW_PARTS; ++i) {
            const HSD_VtxDescList* a = orig[i];
            for (k = 0; k < ROW_ATTRS - 1 && a[k].attr != GX_VA_NULL; ++k) {}
            if (a[k].attr != GX_VA_NULL) return;
            memcpy(g->list[i], a, (k + 1) * sizeof *a);
            for (k = 0; g->list[i][k].attr != GX_VA_NULL; ++k)
                if (g->list[i][k].attr == GX_VA_POS) {
                    g->list[i][k].frac = (u8) frac;
                    DISC_SET(g->list[i][k].vertex, g->pos);
                }
            g->orig[i] = orig[i];
        }
        g->src = DP(pos->vertex);
        g->width = width; g->shift = shift; g->lift = lift;
        OSReport("online menu: label %d is %d texels, centre %+d texels (%d positions, frac %d)\n",
                 id, width, shift, count, frac);
    }
    for (i = 0; i < ROW_PARTS; ++i) { g->orig[i] = orig[i]; p[i]->verts = g->list[i]; }
}

/* A label that changes while its row stays hovered (Unranked's slider). The caller finds the joint in
 * the live menu object each time: a joint kept from an earlier frame may belong to a menu already freed
 * (B, a page turn), whose joints HSD hands to the next menu's rows. */
void mu_online_pages_relabel(HSD_JObj* label, int row)
{
    if (label == NULL || row < 0 || row >= 8) return;
    pages_label_swap(label, MENU_KIND_8, row, 0);
}

static int pages_label_swap(HSD_JObj* label, int menu_kind, int selection, int capture)
{
    static HSD_ImageDesc image[MP_L_END];
    HSD_DObj* dobj;
    const unsigned char* art;
    int id, width, shift, quad;
    float lift = 0.0f;
    if (!mu_slippi_online_pages()) return 0;
    if (capture || label == NULL || menu_kind != MENU_KIND_8 || selection < 0 || selection >= 8) return 1;
    id = mu_online_pages_label(selection);
    if (id <= MP_L_NONE || id >= MP_L_END || !(art = pages_art(id))) return 1;
    width = id == MP_L_YG_SQUAD ? MU_SST_SQUAD_W :
            (art == mu_sst_un_yougame || art == mu_sst_un_both || art == mu_sst_un_slippi || art == mu_sst_dbl_ranked) ? MU_SST_UNRANKED_W : MU_LABEL_W;
    shift = id == MP_L_YG_SQUAD ? MU_SST_SQUAD_SHIFT : 0;
    quad = width;
    /* UI audit previews (mu_test_ui.h), in texels of the label (2.2 px each at 1440x1080).
     * 1: Squad Strike's label 4% narrower and back on the native centre, so the cursor ring clears it.
     * 6: the third row's label (Friends, Direct) moved right, off the left cap.
     * 7: Squad Strike's label raised to the other rows' line. 8: Slippi's full-size labels lowered. */
    if (id == MP_L_YG_SQUAD) {   /* owner 11-18: the ring clears the label */ quad = (int) mu_ui_num(0, 192.0f); shift = (int) mu_ui_num(1, 0.0f); }
    if ((id == MP_L_YG_FRIENDS || id == MP_L_SLP_DIRECT) && mu_ui_fix("6")) shift = (int) mu_ui_num(2, 9.0f);
    if (id == MP_L_YG_SQUAD) lift = mu_ui_num(3, 2.5f);
    if ((art == mu_sst_slippi_direct || art == mu_sst_slippi_teams || art == mu_sst_slippi_party) && mu_ui_fix("8"))
        lift = mu_ui_num(4, -1.8f);
    for (dobj = label->u.dobj; dobj != NULL; dobj = dobj->next) {
        HSD_TObj* texture;
        if (dobj->mobj == NULL) continue;
        for (texture = dobj->mobj->tobj; texture != NULL; texture = texture->next) {
            HSD_ImageDesc* original = texture->imagedesc;
            /* The label may already be one of ours, at another width (a row shown again with another label). */
            const int ours = original >= &image[0] && original < &image[MP_L_END];
            if (original == NULL || (!ours && (original->width != MU_LABEL_W || original->height != MU_LABEL_H ||
                original->format != GX_TF_IA4))) continue;
            if (original != &image[id]) image[id] = *original;
            DISC_SET(image[id].image_ptr, art);
            image[id].width = (u16) width;
            texture->imagedesc = &image[id];
        }
    }
    row_geo_apply(label, id, quad, shift, lift);
    return 1;
}

/* The row's own description with words added (the page's Z hint): its prefix and glyphs kept. */
void mu_menu_description_append(HSD_Text* text, const char* message)
{
    unsigned char encoded[512], stream[1024];
    const unsigned char* src;
    SisBuffer* alloc;
    int prefix, n, i, length;
    if (!text || !message || strlen(message) > 60 || !(src = text->sis_buffer)) return;
    prefix = mu_menu_description_prefix(src);
    if (!prefix) return;
    memcpy(stream, src, prefix);
    n = prefix;
    for (i = prefix; i < 600 && n < 700;) {
        unsigned op = src[i];
        int size = op >= 0x20 ? 2 : mu_menu_sis_command_size(op);
        if (!op || op == 8 || op == 9) break;
        if (op >= 0x20 || op == 0x1a) { memcpy(stream + n, src + i, size); n += size; }
        i += size;
    }
    length = HSD_SisLib_803A67EC(encoded, (u8*) message);
    for (i = 0; i < length && n < 1000;) {
        unsigned op = encoded[i];
        int size = op >= 0x20 ? 2 : mu_menu_sis_command_size(op);
        if (op >= 0x20 || op == 0x1a) { memcpy(stream + n, encoded + i, size); n += size; }
        i += size;
    }
    stream[n] = 0;
    alloc = (SisBuffer*) HSD_SisLib_Alloc(sizeof *alloc);
    if (!alloc) return;
    alloc->data = (u8*) HSD_SisLib_Alloc(n + 1);
    if (!alloc->data) { HSD_SisLib_Free(alloc); return; }
    memcpy(alloc->data, stream, n + 1);
    alloc->end = alloc->data + n;
    alloc->size = n + 1;
    alloc->count = 0;
    if (text->alloc_data) {
        HSD_SisLib_Free(text->alloc_data->data);
        HSD_SisLib_Free(text->alloc_data);
    }
    text->alloc_data = alloc;
    text->sis_buffer = alloc->data;
    text->x60 = NULL;
}
