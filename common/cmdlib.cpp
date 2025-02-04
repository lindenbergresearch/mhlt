#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "blockmem.h"
#include "cmdlib.h"
#include "hlassert.h"
#include "log.h"
#include "messages.h"

#ifdef SYSTEM_POSIX
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <cstdarg>
#include <unistd.h>
#endif
#endif

#include <cctype>
#include <clocale>
#include <iostream>
#include <stdio.h>
#include <string.h>

#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')

/*
 * ================
 * I_FloatTime
 * ================
 */

double I_FloatTime() {
#ifdef SYSTEM_WIN32
    FILETIME ftime;
    double rval;

    GetSystemTimeAsFileTime(&ftime);

    rval = ftime.dwLowDateTime;
    rval += ((__int64) ftime.dwHighDateTime) << 32;

    return (rval / 10000000.0);
#endif

#ifdef SYSTEM_POSIX
    struct timeval tp;
    struct timezone tzp;
    static int secbase;

    gettimeofday(&tp, &tzp);

    if (!secbase) {
        secbase = tp.tv_sec;
        return tp.tv_usec / 1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
#endif
}

#ifdef SYSTEM_POSIX
char *strupr(char *string) {
    int i;
    int len = strlen(string);

    for (i = 0; i < len; i++) {
        string[i] = toupper(string[i]);
    }
    return string;
}

char *strlwr(char *string) {
    int i;
    int len = strlen(string);

    for (i = 0; i < len; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
}
#endif

// Case Insensitive substring matching
const char *stristr(const char *const string, const char *const substring) {
    char *string_copy;
    char *substring_copy;
    const char *match;

    string_copy = strdup(string);
    strlwr(string_copy);

    substring_copy = strdup(substring);
    strlwr(substring_copy);

    match = strstr(string_copy, substring_copy);
    if (match) {
        match = (string + (match - string_copy));
    }

    free(string_copy);
    free(substring_copy);
    return match;
}

char *FlipSlashes(char *string) {
    while (*string) {
        if (PATHSEPARATOR(*string)) {
            *string = SYSTEM_SLASH_CHAR;
        }
        string++;
    }
    return string;
}

void DefaultExtension(char *path, const char *extension) {
    char *src;

    //
    // if path doesn't have a .EXT, append extension
    // (extension should include the .)
    //
    src = path + strlen(path) - 1;

    while (!PATHSEPARATOR(*src) && src != path) {
        if (*src == '.')
            return;// it has an extension
        src--;
    }

    strcat(path, extension);
}

void StripFilename(char *path) {
    int length;

    length = strlen(path) - 1;
    while (length > 0 && !PATHSEPARATOR(path[length]))
        length--;
    path[length] = 0;
}

void StripExtension(char *path) {
    int length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != '.') {
        length--;
        if (path[length] == '/')
            return;// no extension
    }
    if (length)
        path[length] = 0;
}

/*
 * ====================
 * Extract file parts
 * ====================
 */
void ExtractFilePath(const char *const path, char *dest) {
    hlassert(path != dest);

    const char *src;

    src = path + strlen(path) - 1;

    //
    // back up until a \ or the start
    //
    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    memcpy(dest, path, src - path);
    dest[src - path] = 0;
}

void ExtractFile(const char *const path, char *dest) {
    hlassert(path != dest);

    const char *src;

    src = path + strlen(path) - 1;

    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    while (*src) {
        *dest++ = *src++;
    }
    *dest = 0;
}

void ExtractFileBase(const char *const path, char *dest) {
    hlassert(path != dest);

    const char *src;

    src = path + strlen(path) - 1;

    //
    // back up until a \ or the start
    //
    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    while (*src && *src != '.') {
        *dest++ = *src++;
    }
    *dest = 0;
}

void ExtractFileExtension(const char *const path, char *dest) {
    hlassert(path != dest);

    const char *src;

    src = path + strlen(path) - 1;

    //
    // back up until a . or the start
    //
    while (src != path && *(src - 1) != '.')
        src--;
    if (src == path) {
        *dest = 0;// no extension
        return;
    }

    strcpy(dest, src);
}

/*
 * ============================================================================
 * 
 * BYTE ORDER FUNCTIONS
 * 
 * ============================================================================
 */

#ifdef WORDS_BIGENDIAN

short LittleShort(const short l) {
    byte b1, b2;

    b1 = l & 255;
    b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}

short BigShort(const short l) {
    return l;
}

int LittleLong(const int l) {
    byte b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

int BigLong(const int l) {
    return l;
}

float LittleFloat(const float l) {
    union {
        byte b[4];
        float f;
    } in, out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float BigFloat(const float l) {
    return l;
}

#else// Little endian (Intel, etc)

short BigShort(const short l) {
    byte b1, b2;

    b1 = (byte) (l & 255);
    b2 = (byte) ((l >> 8) & 255);

    return (short) ((b1 << 8) + b2);
}

short LittleShort(const short l) {
    return l;
}

int BigLong(const int l) {
    byte b1, b2, b3, b4;

    b1 = (byte) (l & 255);
    b2 = (byte) ((l >> 8) & 255);
    b3 = (byte) ((l >> 16) & 255);
    b4 = (byte) ((l >> 24) & 255);

    return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

int LittleLong(const int l) {
    return l;
}

float BigFloat(const float l) {
    union {
        byte b[4];
        float f;
    } in, out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float LittleFloat(const float l) {
    return l;
}

#endif

//=============================================================================

bool CDECL safe_snprintf(char *const dest, const size_t count, const char *const args, ...) {
    size_t amt;
    va_list argptr;

    hlassert(count > 0);

    va_start(argptr, args);
    amt = vsnprintf(dest, count, args, argptr);
    va_end(argptr);

    // truncated (bad!, snprintf doesn't null terminate the string when this happens)
    if (amt == count) {
        dest[count - 1] = 0;
        return false;
    }

    return true;
}

bool safe_strncpy(char *const dest, const char *const src, const size_t count) {
    return safe_snprintf(dest, count, "%s", src);
}

bool safe_strncat(char *const dest, const char *const src, const size_t count) {
    if (count) {
        strncat(dest, src, count);

        dest[count - 1] = 0;// Ensure it is null terminated
        return true;
    } else {
        Warning("safe_strncat passed empty count");
        return false;
    }
}

bool TerminatedString(const char *buffer, const int size) {
    int x;

    for (x = 0; x < size; x++, buffer++) {
        if ((*buffer) == 0) {
            return true;
        }
    }
    return false;
}
