#include "playlist.h"
#include "util_uart.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* Global playlist storage */
Track    g_tracks[MAX_TRACKS];
uint32_t g_track_count = 0;

/* ===== Internal helpers ===== */
static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\0'))
        s[--n] = '\0';
}

static int ends_with_ci(const char *s, const char *suf)
{
    size_t ls = strlen(s), lsf = strlen(suf);
    if (ls < lsf) return 0;
    for (size_t i = 0; i < lsf; ++i) {
        char a = s[ls - lsf + i];
        char b = suf[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

static int ci_strcmp(const char *a, const char *b)
{
    while (*a || *b) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return (int)ca - (int)cb;
        if (*a) ++a;
        if (*b) ++b;
    }
    return 0;
}

static int cmp_tracks(const void *A, const void *B)
{
    const Track *a = (const Track *)A;
    const Track *b = (const Track *)B;
    return ci_strcmp(a->path, b->path);
}

/* ===== ID3v1 parser ===== */
int ReadID3v1(FIL *f, ID3v1 *out)
{
    if (f_size(f) < 128) return 0;
    FSIZE_t cur = f_tell(f);
    if (f_lseek(f, f_size(f) - 128) != FR_OK) return 0;

    UINT br = 0;
    uint8_t tag[128];
    if (f_read(f, tag, 128, &br) != FR_OK || br != 128) {
        (void)f_lseek(f, cur);
        return 0;
    }
    if (memcmp(tag, "TAG", 3) != 0) {
        (void)f_lseek(f, cur);
        return 0;
    }
    memcpy(out->title, tag + 3, 30);
    out->title[30] = 0;
    rtrim(out->title);

    memcpy(out->artist, tag + 33, 30);
    out->artist[30] = 0;
    rtrim(out->artist);

    (void)f_lseek(f, cur);
    return 1;
}

/* ===== Shuffle ===== */
static uint32_t g_rng = 0xC001BEEF;

uint32_t xorshift32(void)
{
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x ? x : 0x1234567U;
    return g_rng;
}

void ShuffleTracks(void)
{
    if (g_track_count < 2) return;
    for (int i = (int)g_track_count - 1; i > 0; --i) {
        int j = (int)(xorshift32() % (uint32_t)(i + 1));
        if (j != i) {
            Track tmp = g_tracks[i];
            g_tracks[i] = g_tracks[j];
            g_tracks[j] = tmp;
        }
    }
}

/* ===== Playlist builder ===== */
void BuildPlaylist(const char *dir)
{
    DIR d;
    FILINFO fno;
    g_track_count = 0;

#if FF_USE_LFN
    static char lfn[256];
    fno.lfname = lfn;
    fno.lfsize = sizeof(lfn);
#endif

    if (f_opendir(&d, dir) != FR_OK) {
        uprintln("ERR: cannot open %s", dir);
        return;
    }

    for (;;) {
        FRESULT fr = f_readdir(&d, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

#if FF_USE_LFN
        const char *name = (fno.lfname && fno.lfname[0]) ? fno.lfname : fno.fname;
#else
        const char *name = fno.fname;
#endif

        if (fno.fattrib & AM_DIR) continue;
        if (!ends_with_ci(name, ".mp3")) continue;

        if (g_track_count < MAX_TRACKS) {
            snprintf(g_tracks[g_track_count].path,
                     sizeof(g_tracks[g_track_count].path),
                     "%s/%s", dir, name);
            g_tracks[g_track_count].title[0] = 0;
            g_tracks[g_track_count].artist[0] = 0;
            g_track_count++;
        } else {
            uprintln("Playlist full (%d)", MAX_TRACKS);
            break;
        }
    }

    f_closedir(&d);

    if (g_track_count > 1)
        qsort(g_tracks, g_track_count, sizeof(Track), cmp_tracks);

    uprintln("Playlist built: %lu track(s)", (unsigned long)g_track_count);
}
