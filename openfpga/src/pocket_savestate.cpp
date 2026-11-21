/* Pocket Memories and sleep for VVVVVV (openfpgaOS save-state service).
 *
 * A Memory holds a main-game save, the same XML the game writes for its own
 * quicksave, taken at the player's current spot when the player has control
 * and no script is running, or at the last checkpoint otherwise (which is
 * what the game's own save does in cutscenes).  Loading it starts the game
 * from that save, as Continue does.  The respawn checkpoint is kept, so a
 * death after loading goes back to the real checkpoint.
 *
 * Everything the game itself refuses to quicksave (time trials, No Death
 * Mode, intermissions, the Secret Lab, custom levels, the editor) and the
 * menus are saved as a "title" Memory, which loads to the title screen.
 * Never refusing matters for sleep: a refused sleep save still powers off,
 * and the next wake would bring back an older state.
 *
 * core.json "sleep_supported" must be true for the Pocket to ask at all;
 * scripts/package.sh keeps it false for the benchmark core, which does not
 * link this in.
 */

#if defined(VVV_POCKET) && !defined(POCKET_BENCH)

#include <stdint.h>
#include <string.h>

#include <tinyxml2.h>

#include "of.h"
#include "of_savestate.h"

#include "../../desktop_version/src/Entity.h"
#include "../../desktop_version/src/Game.h"
#include "../../desktop_version/src/Graphics.h"
#include "../../desktop_version/src/Map.h"
#include "../../desktop_version/src/Music.h"
#include "../../desktop_version/src/Script.h"
#include "../../desktop_version/src/UtilityClass.h"
#include "../../desktop_version/src/Vlogging.h"

#include "pocket_savestate.h"

/* Read by Game::loadquick() while a Memory is being restored. */
extern tinyxml2::XMLDocument* POCKET_memory_doc;

namespace
{

const uint32_t MEMORY_MAGIC = 0x314d5656u; /* "VVM1" */
const uint32_t KIND_TITLE = 0;
const uint32_t KIND_GAME = 1;
const uint32_t MAX_STATE = 64 * 1024;

struct MemoryHeader
{
    uint32_t magic;
    uint32_t kind;
    /* The respawn checkpoint at the time of the save. */
    int32_t savex, savey, saverx, savery, savegc, savedir;
    uint32_t xml_len;
};

unsigned char state_buf[MAX_STATE];
bool enabled = false;

bool can_save_game(void)
{
    if (map.custommode || game.inspecial())
    {
        return false;
    }
    switch (game.gamestate)
    {
    case GAMEMODE:
    case MAPMODE:
    case TELEPORTERMODE:
        return !game.ingame_titlemode;
    default:
        return false;
    }
}

/* The player's current spot, when standing still there is a safe restart. */
bool can_save_here(int player)
{
    return game.gamestate == GAMEMODE
        && game.hascontrol
        && !script.running
        && game.deathseq == -1
        && game.lifeseq == 0
        && !game.completestop
        && INBOUNDS_VEC(player, obj.entities);
}

int build_state(void)
{
    MemoryHeader header;
    SDL_zero(header);
    header.magic = MEMORY_MAGIC;
    header.kind = KIND_TITLE;

    if (!can_save_game())
    {
        SDL_memcpy(state_buf, &header, sizeof(header));
        return (int) sizeof(header);
    }

    header.kind = KIND_GAME;
    header.savex = game.savex;
    header.savey = game.savey;
    header.saverx = game.saverx;
    header.savery = game.savery;
    header.savegc = game.savegc;
    header.savedir = game.savedir;

    const int player = obj.getplayer();
    if (can_save_here(player))
    {
        /* As the "savepoint" script command does. */
        game.savex = obj.entities[player].xp;
        game.savey = obj.entities[player].yp;
        game.savegc = game.gravitycontrol;
        game.saverx = game.roomx;
        game.savery = game.roomy;
        game.savedir = obj.entities[player].dir;
    }

    tinyxml2::XMLDocument doc;
    game.writemaingamesave(doc);

    game.savex = header.savex;
    game.savey = header.savey;
    game.saverx = header.saverx;
    game.savery = header.savery;
    game.savegc = header.savegc;
    game.savedir = header.savedir;

    tinyxml2::XMLPrinter printer(NULL, true);
    doc.Print(&printer);
    const int xml_len = printer.CStrSize(); /* includes the terminator */
    if (xml_len <= 0 || sizeof(header) + (size_t) xml_len > sizeof(state_buf))
    {
        vlog_error("Memory too large: %d bytes", xml_len);
        return -1;
    }
    header.xml_len = (uint32_t) xml_len;
    SDL_memcpy(state_buf, &header, sizeof(header));
    SDL_memcpy(state_buf + sizeof(header), printer.CStr(), xml_len);
    return (int) (sizeof(header) + xml_len);
}

bool restore_state(int len)
{
    MemoryHeader header;
    if (len < (int) sizeof(header))
    {
        return false;
    }
    SDL_memcpy(&header, state_buf, sizeof(header));
    if (header.magic != MEMORY_MAGIC)
    {
        return false;
    }

    if (header.kind == KIND_TITLE)
    {
        if (game.gamestate != TITLEMODE)
        {
            game.quittomenu();
            music.play(Music_PRESENTINGVVVVVV);
        }
        return true;
    }
    if (header.kind != KIND_GAME
    || header.xml_len == 0
    || sizeof(header) + header.xml_len != (uint32_t) len
    || state_buf[len - 1] != '\0')
    {
        return false;
    }

    tinyxml2::XMLDocument doc;
    doc.Parse((const char*) state_buf + sizeof(header), header.xml_len - 1);
    if (doc.Error())
    {
        vlog_error("Memory does not parse: %s", doc.ErrorStr());
        return false;
    }

    /* The game's Continue path, reading this save instead of qsave.vvv. */
    POCKET_memory_doc = &doc;
    script.startgamemode(Start_MAINGAME_QUICKSAVE);
    POCKET_memory_doc = NULL;

    game.savex = header.savex;
    game.savey = header.savey;
    game.saverx = header.saverx;
    game.savery = header.savery;
    game.savegc = header.savegc;
    game.savedir = header.savedir;
    return true;
}

} /* namespace */

void POCKET_SavestateInit(void)
{
#ifdef MAKEANDPLAY
    static const char id[] = "VVVVVV-mp";
#else
    static const char id[] = "VVVVVV-main";
#endif
    of_savestate_set_id(id, sizeof(id) - 1);
    enabled = of_savestate_enable(MAX_STATE) == 0;
    if (!enabled)
    {
        vlog_info("Pocket Memories unavailable on this runtime");
    }
}

void POCKET_SavestatePoll(void)
{
    /* The preloader moves itself to the title when it ends, which would
     * undo a restored game: answer once it is over (the OS holds a wake's
     * load for 30 seconds). */
    if (!enabled || game.gamestate == PRELOADER)
    {
        return;
    }
    switch (of_savestate_poll())
    {
    case OF_SAVESTATE_SAVE:
    {
        const int n = build_state();
        if (n > 0)
        {
            of_savestate_save(state_buf, (uint32_t) n);
        }
        else
        {
            of_savestate_finish(0);
        }
        break;
    }
    case OF_SAVESTATE_LOAD:
    {
        const int n = of_savestate_load(state_buf, sizeof(state_buf));
        if (n > 0)
        {
            of_savestate_finish(restore_state(n) ? 1 : 0);
        }
        break;
    }
    default:
        break;
    }
}

#endif
