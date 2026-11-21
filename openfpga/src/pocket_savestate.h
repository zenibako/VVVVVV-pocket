#ifndef POCKET_SAVESTATE_H
#define POCKET_SAVESTATE_H

/* Pocket Memories and sleep (see pocket_savestate.cpp). */
void POCKET_SavestateInit(void);
/* Once per logic step, where the game's state is consistent. */
void POCKET_SavestatePoll(void);

#endif /* POCKET_SAVESTATE_H */
