/*
 * Copyright (C) 2012 Hubert Kario <kario@wsisiz.edu.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */
#include <stdlib.h>
#include <assert.h>
#include "extents.h"

void
free_extents(struct extents *e)
{
    if (!e)
        return;

    free(e->extents);

    free(e);
}

void
free_extent_stats(struct extent_stats *es)
{
    if (!es)
        return;

    free(es);
}

void
free_extent(struct extent *e)
{
    // do nothing, extent is freed after in free_extents()
    return;
}

// strcmp for extents
int
compare_extents(struct extents *e1, struct extents *e2)
{
    assert(e1);
    assert(e2);

    for(size_t i=0; i < e1->length && e2->length && i < SIZE_MAX; i++) {
        if (e1->extents[i].score == e2->extents[i].score)
          continue;

        if (e1->extents[i].score > e2->extents[i].score)
          return 1;

        // if (e1->extents[i].score < e2->extents[i].score)
        return -1;
    }

    if (e1->length < e2->length)
      return -1;
    if (e1->length > e2->length)
      return 1;

    return 0;
}

float
get_extent_score(struct extent *e)
{
    assert(e);
    return e->score;
}

struct extent *
get_extent(struct extents *e, size_t nmemb)
{
    assert(e);
    assert(e->length < nmemb);
    return &(e->extents[nmemb]);
}

int
count_extents(struct extents *e, float score, int hot_cold)
{
    size_t count = 0;
    for(size_t i=0; i<e->length; i++) {
        if (hot_cold == ES_COLD && e->extents[i].score > score)
            count++;
        if (hot_cold == ES_HOT && e->extents[i].score < score)
            count++;
    }

    return count;
}

void
truncate_extents(struct extents *e, size_t len)
{
    assert(e);
    assert(e->extents);

    if (e->length < len)
      return;

    struct extent *ext;

    ext = realloc(e->extents, sizeof(struct extent) * len);

    if (!ext) {
        free(e->extents);
        e->extents = NULL;
        e->length = 0;
        return;
    }

    e->extents = ext;
    e->length = len;
}