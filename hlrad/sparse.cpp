#include "qrad.h"

// Transparency array

#ifdef HLRAD_HULLU

typedef struct {
    unsigned x;
    unsigned y;
    vec3_t transparency;
} transparency_t;
static transparency_t *s_transparency_list = NULL;
static unsigned long s_transparency_count = 0;
static unsigned long s_max_transparency_count = 0;

static void FindOpacity(const unsigned p1, const unsigned p2, vec3_t &out) {
    for (unsigned long i = 0; i < s_transparency_count; i++) {
        if (s_transparency_list[i].x == p1 && s_transparency_list[i].y == p2) {
            VectorCopy(s_transparency_list[i].transparency, out);
            return;
        }
    }
    VectorFill(out, 1.0);
}

#endif /*HLRAD_HULLU*/

typedef struct {
    unsigned offset : 24;
    unsigned values : 8;
} sparse_row_t;

typedef struct {
    sparse_row_t *row;
    int count;
} sparse_column_t;

sparse_column_t *s_vismatrix;

// Vismatrix protected
static unsigned IsVisbitInArray(const unsigned x, const unsigned y) {
    int first, last, current;
    int y_byte = y / 8;
    sparse_row_t *row;
    sparse_column_t *column = s_vismatrix + x;

    if (!column->count) {
        return -1;
    }

    first = 0;
    last = column->count - 1;

    //    Warning("Searching . . .");
    // binary search to find visbit
    while (1) {
        current = (first + last) / 2;
        row = column->row + current;
        //        Warning("first %u, last %u, current %u, row %p, row->offset %u", first, last, current, row, row->offset);
        if ((row->offset) < y_byte) {
            first = current + 1;
        } else if ((row->offset) > y_byte) {
            last = current - 1;
        } else {
            return current;
        }
        if (first > last) {
            return -1;
        }
    }
}

// Vismatrix protected
static void InsertVisbitIntoArray(const unsigned x, const unsigned y) {
    unsigned count;
    unsigned y_byte = y / 8;
    sparse_column_t *column = s_vismatrix + x;
    sparse_row_t *row = column->row;

    if (!column->count) {
        column->count++;
        row = column->row = (sparse_row_t *) malloc(sizeof(sparse_row_t));
        row->offset = y_byte;
        row->values = 1 << (y & 7);
        return;
    }

    // Insertion
    count = 0;
    while (count < column->count) {
        if (row->offset > y_byte) {
            unsigned newsize = (column->count + 1) * sizeof(sparse_row_t);
            sparse_row_t *newrow = (sparse_row_t *) malloc(newsize);

            memcpy(newrow, column->row, count * sizeof(sparse_row_t));
            memcpy(newrow + count + 1, column->row + count, (column->count - count) * sizeof(sparse_row_t));

            row = newrow + count;
            row->offset = y_byte;
            row->values = 1 << (y & 7);

            free(column->row);
            column->row = newrow;
            column->count++;
            return;
        }

        row++;
        count++;
    }

    // Append
    {
        unsigned newsize = (count + 1) * sizeof(sparse_row_t);
        sparse_row_t *newrow = (sparse_row_t *) malloc(newsize);

        memcpy(newrow, column->row, column->count * sizeof(sparse_row_t));

        row = newrow + column->count;
        row->offset = y_byte;
        row->values = 1 << (y & 7);

        free(column->row);
        column->row = newrow;
        column->count++;
        return;
    }
}

// Vismatrix public
static void SetVisBit(unsigned x, unsigned y) {
    unsigned offset;

    if (x == y) {
        return;
    }

    if (x > y) {
        const unsigned a = x;
        const unsigned b = y;
        x = b;
        y = a;
    }

    if (x > g_num_patches) {
        Warning("in SetVisBit(), x > num_patches");
    }
    if (y > g_num_patches) {
        Warning("in SetVisBit(), y > num_patches");
    }

    ThreadLock();

    if ((offset = IsVisbitInArray(x, y)) != -1) {
        s_vismatrix[x].row[offset].values |= 1 << (y & 7);
    } else {
        InsertVisbitIntoArray(x, y);
    }

    ThreadUnlock();
}

// Vismatrix public
#ifdef HLRAD_HULLU
static bool CheckVisBitSparse(unsigned x, unsigned y, vec3_t &transparency_out)
#else
static bool CheckVisBitSparse(unsigned x, unsigned y)
#endif
{
    unsigned offset;

    if (x == y) {
#ifdef HLRAD_HULLU
        VectorFill(transparency_out, 1.0);
#endif
        return 1;
    }

    if (x > y) {
        const unsigned a = x;
        const unsigned b = y;
        x = b;
        y = a;
    }

    if (x > g_num_patches) {
        Warning("in CheckVisBit(), x > num_patches");
    }
    if (y > g_num_patches) {
        Warning("in CheckVisBit(), y > num_patches");
    }

    if ((offset = IsVisbitInArray(x, y)) != -1) {
#ifdef HLRAD_HULLU
        if (g_customshadow_with_bouncelight) {
            vec3_t tmp = {1.0, 1.0, 1.0};
            FindOpacity(x, y, tmp);
            VectorCopy(tmp, transparency_out);
        } else {
            VectorFill(transparency_out, 1.0);
        }
#endif
        return s_vismatrix[x].row[offset].values & (1 << (y & 7));
    }
#ifdef HLRAD_HULLU
    VectorFill(transparency_out, 1.0);
#endif
    return 0;
}

/*
 * ==============
 * TestPatchToFace
 * 
 * Sets vis bits for all patches in the face
 * ==============
 */
static void TestPatchToFace(const unsigned patchnum, const int facenum, const int head) {
    patch_t *patch = &g_patches[patchnum];
    patch_t *patch2 = g_face_patches[facenum];

    // if emitter is behind that face plane, skip all patches

    if (patch2) {
        const dplane_t *plane2 = getPlaneFromFaceNumber(facenum);

        if (DotProduct(patch->origin, plane2->normal) > (PatchPlaneDist(patch2) + MINIMUM_PATCH_DISTANCE)) {
            // we need to do a real test
            const dplane_t *plane = getPlaneFromFaceNumber(patch->faceNumber);

            for (; patch2; patch2 = patch2->next) {
                unsigned m = patch2 - g_patches;

#ifdef HLRAD_HULLU
                vec3_t transparency = {1.0, 1.0, 1.0};
#endif

                // check vis between patch and patch2
                // if bit has not already been set
                //  && v2 is not behind light plane
                //  && v2 is visible from v1
                if (m > patchnum && (DotProduct(patch2->origin, plane->normal) > (PatchPlaneDist(patch) + MINIMUM_PATCH_DISTANCE)) && (TestLine_r(head, patch->origin, patch2->origin) == CONTENTS_EMPTY)
#ifdef HLRAD_HULLU
                    && (!TestSegmentAgainstOpaqueList(patch->origin, patch2->origin, transparency)))
#else
                    && (!TestSegmentAgainstOpaqueList(patch->origin, patch2->origin)))
#endif
                {

#ifdef HLRAD_HULLU
                    // transparency face fix table
                    if (g_customshadow_with_bouncelight && fabs(VectorAvg(transparency) - 1.0) < 0.001) {
                        while (s_transparency_count >= s_max_transparency_count) {
                            //new size
                            unsigned long oldsize = s_max_transparency_count;
                            s_max_transparency_count += 128;

                            //realloc
                            s_transparency_list = (transparency_t *) realloc(s_transparency_list, s_max_transparency_count * sizeof(transparency_t));

                            // clean new memory
                            memset(&s_transparency_list[oldsize], 0, sizeof(transparency_t) * 128);
                        }

                        //add to array
                        VectorCopy(transparency, s_transparency_list[s_transparency_count].transparency);
                        s_transparency_list[s_transparency_count].y = m;
                        s_transparency_list[s_transparency_count].x = patchnum;

                        s_transparency_count++;
                    }
#endif
                    SetVisBit(m, patchnum);
                }
            }
        }
    }
}

/*
 * ==============
 * BuildVisRow
 * 
 * Calc vis bits from a single patch
 * ==============
 */
static void BuildVisRow(const int patchnum, byte *pvs, const int head) {
    int j, k, l;
    byte face_tested[MAX_MAP_FACES];
    dleaf_t *leaf;

    memset(face_tested, 0, g_numfaces);

    // leaf 0 is the solid leaf (skipped)
    for (j = 1, leaf = g_dleafs + 1; j < g_numleafs; j++, leaf++) {
        if (!(pvs[(j - 1) >> 3] & (1 << ((j - 1) & 7)))) {
            continue;// not in pvs
        }
        for (k = 0; k < leaf->nummarksurfaces; k++) {
            l = g_dmarksurfaces[leaf->firstmarksurface + k];

            // faces can be marksurfed by multiple leaves, but
            // don't bother testing again
            if (face_tested[l])
                continue;
            face_tested[l] = 1;

            TestPatchToFace(patchnum, l, head);
        }
    }
}

/*
 * ===========
 * BuildVisLeafs
 * 
 * This is run by multiple threads
 * ===========
 */
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable : 4100)// unreferenced formal parameter
#endif
static void BuildVisLeafs(int threadnum) {
    int i;
    int lface, facenum, facenum2;
    byte pvs[(MAX_MAP_LEAFS + 7) / 8];
    dleaf_t *srcleaf;
    dleaf_t *leaf;
    patch_t *patch;
    int head;
    unsigned patchnum;

    while (1) {
        //
        // build a minimal BSP tree that only
        // covers areas relevent to the PVS
        //
        i = GetThreadWork();
        if (i == -1) {
            break;
        }
        i++;// skip leaf 0
        srcleaf = &g_dleafs[i];
        DecompressVis(&g_dvisdata[srcleaf->visofs], pvs, sizeof(pvs));
        head = 0;

        //
        // go through all the faces inside the
        // leaf, and process the patches that
        // actually have origins inside
        //
        for (lface = 0; lface < srcleaf->nummarksurfaces; lface++) {
            facenum = g_dmarksurfaces[srcleaf->firstmarksurface + lface];
            for (patch = g_face_patches[facenum]; patch; patch = patch->next) {
                leaf = PointInLeaf(patch->origin);
                if (leaf != srcleaf) {
                    continue;
                }

                patchnum = patch - g_patches;
                // build to all other world leafs
                BuildVisRow(patchnum, pvs, head);

                // build to bmodel faces
                if (g_nummodels < 2) {
                    continue;
                }
                for (facenum2 = g_dmodels[1].firstface; facenum2 < g_numfaces; facenum2++) {
                    TestPatchToFace(patchnum, facenum2, head);
                }
            }
        }
    }
}

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

/*
 * ==============
 * BuildVisMatrix
 * ==============
 */
static void BuildVisMatrix() {
    s_vismatrix = (sparse_column_t *) AllocBlock(g_num_patches * sizeof(sparse_column_t));

    if (!s_vismatrix) {
        Log("Failed to allocate vismatrix");
        hlassume(s_vismatrix != NULL, assume_NoMemory);
    }

    NamedRunThreadsOn(g_numleafs - 1, g_estimate, BuildVisLeafs);
}

static void FreeVisMatrix() {
    if (s_vismatrix) {
        unsigned x;
        sparse_column_t *item;

        for (x = 0, item = s_vismatrix; x < g_num_patches; x++, item++) {
            if (item->row) {
                free(item->row);
            }
        }
        if (FreeBlock(s_vismatrix)) {
            s_vismatrix = NULL;
        } else {
            Warning("Unable to free vismatrix");
        }
    }

#ifdef HLRAD_HULLU
    if (s_transparency_list) {
        free(s_transparency_list);
        s_transparency_list = NULL;
    }
    s_transparency_count = s_max_transparency_count = 0;
#endif
}

static void DumpVismatrixInfo() {
    unsigned totals[8];
    unsigned total_vismatrix_memory = sizeof(sparse_column_t) * g_num_patches;

    sparse_column_t *column_end = s_vismatrix + g_num_patches;
    sparse_column_t *column = s_vismatrix;

    memset(totals, 0, sizeof(totals));

    while (column < column_end) {
        total_vismatrix_memory += column->count * sizeof(sparse_row_t);
        column++;
    }

    Log("%-20s: %5.1f megs\n", "visibility matrix", total_vismatrix_memory / (1024 * 1024.0));
}

//
// end old vismat.c
////////////////////////////

void MakeScalesSparseVismatrix() {
    char transferfile[_MAX_PATH];

    hlassume(g_num_patches < MAX_SPARSE_VISMATRIX_PATCHES, assume_MAX_PATCHES);

    safe_strncpy(transferfile, g_source, _MAX_PATH);
    StripExtension(transferfile);
    DefaultExtension(transferfile, ".inc");

    if (!g_incremental || !readtransfers(transferfile, g_num_patches)) {
        // determine visibility between g_patches
        BuildVisMatrix();
        DumpVismatrixInfo();
        g_CheckVisBit = CheckVisBitSparse;

#ifdef HLRAD_HULLU
        if ((s_max_transparency_count * sizeof(transparency_t)) >= (1024 * 1024))
            Log("%-20s: %5.1f megs\n", "custom shadow array", (s_max_transparency_count * sizeof(transparency_t)) / (1024 * 1024.0));
        else if (s_transparency_count)
            Log("%-20s: %5.1f kilos\n", "custom shadow array", (s_max_transparency_count * sizeof(transparency_t)) / 1024.0);
#endif

#ifndef HLRAD_HULLU
        NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);
#else
        if (g_rgb_transfers) {
            NamedRunThreadsOn(g_num_patches, g_estimate, MakeRGBScales);
        } else {
            NamedRunThreadsOn(g_num_patches, g_estimate, MakeScales);
        }
#endif
        FreeVisMatrix();

        // invert the transfers for gather vs scatter
#ifndef HLRAD_HULLU
        NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapTransfers);
#else
        if (g_rgb_transfers) {
            NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapRGBTransfers);
        } else {
            NamedRunThreadsOnIndividual(g_num_patches, g_estimate, SwapTransfers);
        }
#endif
        if (g_incremental) {
            writetransfers(transferfile, g_num_patches);
        } else {
            unlink(transferfile);
        }
        // release visibility matrix
        DumpTransfersMemoryUsage();
    }
}
