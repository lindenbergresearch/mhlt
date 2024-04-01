/* common/config.h.  Generated automatically by configure.  */
#ifndef CONFIG_H__
#define CONFIG_H__

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
/* #undef STDC_HEADERS */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <asm/atomic.h> header file. */
/* #undef HAVE_ASM_ATOMIC_H */

/* Define if you have the <sys/atomic.h> header file. */
/* #undef HAVE_SYS_ATOMIC_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Sets the size of char* */
#define SIZEOF_CHARP 0

#if !defined(HAVE_ASM_ATOMIC_H) && !defined(HAVE_SYS_ATOMIC_H)
#define NO_ATOMIC
#endif

#if defined(HAVE_ASM_ATOMIC_H) || defined(HAVE_SYS_ATOMIC_H)
#define HAVE_ATOMIC
#endif

#if !defined(HAVE_PTHREAD_H) || defined(NO_ATOMIC)
#define SINGLE_THREADED
#endif

#ifdef WORDS_BIGENDIAN
#define __BIG_ENDIAN__
#endif

#ifndef WORDS_BIGENDIAN
#endif

#endif /* CONFIG_H__ */
