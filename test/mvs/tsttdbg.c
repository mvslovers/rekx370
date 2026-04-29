/*
 * tsttdbg.c - WP-CPS-01 clock + gmtime64 diagnostic (MVS-only).
 *
 * Isolates whether the DATE('S') garbage values and TIME('L') length
 * failures observed in tsttim after WP-CPS-01 are caused by:
 *   (a) clock64() / uclock64() returning unexpected values
 *   (b) usec fraction (uclock64 - sec64 * 1e6) overflowing at a
 *       second boundary (TIME('L') "wrong length" hypothesis)
 *   (c) gmtime64 misbehaving depending on how time64_t is initialised
 *
 * Three gmtime64 variants discriminate a suspected c2asm370 bug:
 *   A  production path:  time64_t t; t.u64 = sec64;
 *   B  zero-init then:   time64_t t = {0}; t.u64 = sec64;
 *   C  bypass 64-bit:    memset(&t,0,...); t.u32[0]=0; t.u32[1]=(uint32_t)sec64;
 *
 * If A and C agree => production init is fine, DATE('S') failure is elsewhere.
 * If A != C       => c2asm370 64-bit union assign broken on this target.
 * If A == B != C  => zero-init does not rescue the 64-bit assign path.
 *
 * Build:  mbt build --target TSTTDBG
 * Run:    EXEC PGM=TSTTDBG  (see test/mvs/jcl/tsttdbg.jcl)
 *         or: CALL 'hlq.LOAD(TSTTDBG)' from TSO
 */

#include <stdio.h>
#include <string.h>

#ifndef __MVS__

int main(void)
{
    printf("TSTTDBG: MVS-only -- no output on host.\n");
    return 0;
}

#else /* __MVS__ */

#include <time64.h>

#define USEC_PER_SEC  1000000UL
#define TM_YEAR_BASE  1900
#define DATE_S_LEN    8  /* length of DATE('S') result: yyyymmdd */
#define DATE_S_BUFLEN 16 /* sprintf buffer large enough for yyyymmdd */
#define USEC_MAX      999999U

static void print_tm(const char *tag, struct tm *t)
{
    char buf[DATE_S_BUFLEN];
    int n;

    if (t == NULL)
    {
        printf("  %-2s: gmtime64 returned NULL\n", tag);
        return;
    }
    n = sprintf(buf, "%04d%02d%02d",
                t->tm_year + TM_YEAR_BASE, t->tm_mon + 1, t->tm_mday);
    printf("  %-2s: year=%-4d mon=%-2d mday=%-2d "
           "hour=%-2d min=%-2d sec=%-2d\n",
           tag,
           t->tm_year + TM_YEAR_BASE, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
    printf("      DATE('S')='%s' (len=%d, %s)\n",
           buf, n, (n == DATE_S_LEN) ? "OK" : "WRONG");
}

int main(void)
{
    clock64_t sec64 = clock64();
    uint64_t us64 = (uint64_t)uclock64();
    time64_t sec_u; /* union view of sec64 for word printing */
    time64_t us_u;  /* union view of us64 */
    unsigned int usec;
    struct tm *p;

    /* use union assignment to extract 32-bit halves for printing without
     * a 64-bit right-shift (avoids potential c2asm370 shift emulation) */
    sec_u.u64 = (uint64_t)sec64;
    us_u.u64 = us64;

    (void)fflush(stdout);
    printf("TSTTDBG: WP-CPS-01 clock+gmtime64 diagnostic\n");
    printf("==============================================\n");
    (void)fflush(stdout);

    /* --- 1. Raw clock values ---------------------------------------- */
    printf("1. clock64()  hi=%08lX lo=%08lX "
           "array=%04X %04X %04X %04X\n",
           (unsigned long)sec_u.u32[0], (unsigned long)sec_u.u32[1],
           (unsigned)sec_u.array[0], (unsigned)sec_u.array[1],
           (unsigned)sec_u.array[2], (unsigned)sec_u.array[3]);
    printf("2. uclock64() hi=%08lX lo=%08lX "
           "array=%04X %04X %04X %04X\n",
           (unsigned long)us_u.u32[0], (unsigned long)us_u.u32[1],
           (unsigned)us_u.array[0], (unsigned)us_u.array[1],
           (unsigned)us_u.array[2], (unsigned)us_u.array[3]);

    /* --- 2. usec fraction (TIME('L') suspect) ----------------------- */
    usec = (unsigned int)(us64 - (uint64_t)sec64 * (uint64_t)USEC_PER_SEC);
    printf("3. usec fraction = %lu  (%s)\n",
           (unsigned long)usec,
           (usec <= USEC_MAX) ? "OK (0..999999)"
                              : "OVERFLOW -- second-boundary race");

    /* --- 3a. gmtime64: production path — uninit union, t.u64=sec64 -- */
    printf("4. gmtime64 variant A (production: uninit t, t.u64=sec64)\n");
    {
        time64_t t;
        t.u64 = sec64;
        printf("   t.array=%04X %04X %04X %04X\n",
               (unsigned)t.array[0], (unsigned)t.array[1],
               (unsigned)t.array[2], (unsigned)t.array[3]);
        p = gmtime64(&t);
        print_tm("A", p);
    }

    /* --- 3b. gmtime64: zero-init then t.u64=sec64 ------------------- */
    printf("5. gmtime64 variant B (t={0}, t.u64=sec64)\n");
    {
        time64_t t = {0};
        t.u64 = sec64;
        printf("   t.array=%04X %04X %04X %04X\n",
               (unsigned)t.array[0], (unsigned)t.array[1],
               (unsigned)t.array[2], (unsigned)t.array[3]);
        p = gmtime64(&t);
        print_tm("B", p);
    }

    /* --- 3c. gmtime64: direct u32 halves, no 64-bit assign at all --- */
    /* u32[0]=0 is valid until year 2106 (epoch seconds < 2^32).       */
    printf("6. gmtime64 variant C (direct u32 halves, no 64-bit assign)\n");
    {
        time64_t t;
        memset(&t, 0, sizeof(t));
        t.u32[0] = 0U;
        t.u32[1] = (uint32_t)sec64;
        printf("   t.array=%04X %04X %04X %04X\n",
               (unsigned)t.array[0], (unsigned)t.array[1],
               (unsigned)t.array[2], (unsigned)t.array[3]);
        p = gmtime64(&t);
        print_tm("C", p);
    }

    (void)fflush(stdout);
    printf("TSTTDBG: done\n");
    (void)fflush(stdout);
    return 0;
}

#endif /* __MVS__ */
