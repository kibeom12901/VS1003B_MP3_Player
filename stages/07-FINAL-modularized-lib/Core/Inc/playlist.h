#ifndef __PLAYLIST_H
#define __PLAYLIST_H
#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"
#include <stdint.h>

#define MAX_TRACKS 128

typedef struct {
    char path[260];     // e.g. "/music/filename.mp3"
    char title[64];     // ID3v1 Title (optional)
    char artist[64];    // ID3v1 Artist (optional)
} Track;

typedef struct {
    char title[31];
    char artist[31];
} ID3v1;

/* Playlist globals */
extern Track    g_tracks[MAX_TRACKS];
extern uint32_t g_track_count;

/* Functions */
void     BuildPlaylist(const char *dir);
int      ReadID3v1(FIL *f, ID3v1 *out);
void     ShuffleTracks(void);
uint32_t xorshift32(void);

#ifdef __cplusplus
}
#endif
#endif /* __PLAYLIST_H */
