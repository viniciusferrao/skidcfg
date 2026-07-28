#include <stddef.h>

#include "drivers.h"

/* src/drvtab.h holds both tables and is read once per table, with the macro
 * for the other one left to expand to nothing. It undefines both on the way
 * out, which is why each include defines only the one it wants.
 *
 * This is the only thing in the tree that knows how many entries there are or
 * what order they are in, and it learns both by counting what the table file
 * says rather than by being told. */

#define DRV_VIDEO(index, command, brief, label, disk, help) \
    {index, command, brief, label, disk, help, NULL, NULL, 0},
static const struct drv_opt video_opt[] = {
#include "drvtab.h"
};

/* A sound row has no install disk. Line 4 of SETUP.DAT names where the chosen
 * video mode's code files are, and a sound driver has nothing to say about
 * it. */
#define DRV_SOUND(index, command, needs, brief, label, help) \
    {index, command, brief, label, NULL, help, NULL, needs, 0},
static const struct drv_opt sound_opt[] = {
#include "drvtab.h"
};

/* sizeof rather than a constant, so adding or removing an entry costs nothing
 * but the line that does it. The trailing comma the row macro leaves behind is
 * what lets the table file be nothing but rows; C89 allows it. */
const struct drv_tab drv_video = {video_opt,
                                  (int)(sizeof video_opt / sizeof video_opt[0]),
                                  DRV_VIDEO_FALLBACK};

const struct drv_tab drv_sound = {sound_opt,
                                  (int)(sizeof sound_opt / sizeof sound_opt[0]),
                                  DRV_SOUND_FALLBACK};

int drv_rows(const struct drv_tab *t)
{
    int rows = 0;
    int i;

    for (i = 0; i < t->n; i++) {
        if (t->opt[i].label != NULL && !t->opt[i].hidden) {
            rows++;
        }
    }
    return rows;
}

const struct drv_opt *drv_at(const struct drv_tab *t, int row)
{
    int i;

    for (i = 0; i < t->n; i++) {
        if (t->opt[i].label != NULL && !t->opt[i].hidden && row-- == 0) {
            return &t->opt[i];
        }
    }
    return &t->opt[0]; /* see drivers.h */
}

const struct drv_opt *drv_find(const struct drv_tab *t, int index)
{
    int i;

    for (i = 0; i < t->n; i++) {
        if (t->opt[i].index == index) {
            return &t->opt[i];
        }
    }
    return NULL;
}

int drv_row_of(const struct drv_tab *t, int index)
{
    int row = 0;
    int i;

    for (i = 0; i < t->n; i++) {
        if (t->opt[i].index == index) {
            return (t->opt[i].label == NULL || t->opt[i].hidden) ? 0 : row;
        }
        if (t->opt[i].label != NULL && !t->opt[i].hidden) {
            row++;
        }
    }
    return 0;
}
