#ifndef __PLAYER_H
#define __PLAYER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "playlist.h"
#include <stdint.h>

int PlayTrack(Track *t);
int TryMount(void);
void SD_SPI2_Check(void);

#ifdef __cplusplus
}
#endif
#endif /* __PLAYER_H */
