/**
 * Prime Sieve ROM for gxtest
 *
 * Computes the first 100 prime numbers using the Sieve of Eratosthenes
 * and stores them in work RAM for verification by the test harness.
 *
 * Memory layout (work RAM at 0xFF0000):
 *   0xFF0000 - 0xFF0257: Sieve array (600 bytes, 1 = composite)
 *   0xFF0300 - 0xFF03C7: Prime results (100 x 16-bit words)
 *   0xFF0500:            Prime count (16-bit word)
 *   0xFF0502:            Done flag (0xDEAD when complete)
 */

/* Bare-metal type definitions (no standard library) */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/* Memory-mapped locations in work RAM */
#define SIEVE_ARRAY   ((volatile uint8_t*)0xFF0000)
#define PRIME_RESULTS ((volatile uint16_t*)0xFF0300)
#define PRIME_COUNT   ((volatile uint16_t*)0xFF0500)
#define DONE_FLAG     ((volatile uint16_t*)0xFF0502)

/* Constants */
#define SIEVE_SIZE    600   /* Check numbers 0-599 */
#define NUM_PRIMES    100   /* Find first 100 primes */
#define DONE_VALUE    0xDEAD

/**
 * Initialize the sieve array to all zeros (0 = potentially prime)
 */
__attribute__((noinline))
static void clear_sieve(void)
{
    for (int i = 0; i < SIEVE_SIZE; i++) {
        SIEVE_ARRAY[i] = 0;
    }
}

/**
 * Mark 0 and 1 as composite (not prime)
 */
__attribute__((noinline))
static void mark_trivial_composites(void)
{
    SIEVE_ARRAY[0] = 1;  /* 0 is not prime */
    SIEVE_ARRAY[1] = 1;  /* 1 is not prime */
}

/**
 * Run the Sieve of Eratosthenes algorithm
 * For each prime p, mark all multiples of p as composite
 */
__attribute__((noinline))
static void run_sieve(void)
{
    /* Only need to check up to sqrt(SIEVE_SIZE) ≈ 24 */
    for (int p = 2; p <= 24; p++) {
        /* Skip if already marked as composite */
        if (SIEVE_ARRAY[p]) {
            continue;
        }

        /* Mark all multiples of p as composite */
        for (int multiple = p * 2; multiple < SIEVE_SIZE; multiple += p) {
            SIEVE_ARRAY[multiple] = 1;
        }
    }
}

/**
 * Collect the first NUM_PRIMES primes from the sieve into the results array
 */
__attribute__((noinline))
static void collect_primes(void)
{
    int count = 0;

    for (int n = 2; n < SIEVE_SIZE && count < NUM_PRIMES; n++) {
        if (SIEVE_ARRAY[n] == 0) {
            /* n is prime */
            PRIME_RESULTS[count] = n;
            count++;
        }
    }

    /* Store the count */
    *PRIME_COUNT = count;
}

/**
 * Main entry point
 */
void main(void)
{
    /* Step 1: Clear the sieve array */
    clear_sieve();

    /* Step 2: Mark 0 and 1 as not prime */
    mark_trivial_composites();

    /* Step 3: Run the Sieve of Eratosthenes */
    run_sieve();

    /* Step 4: Collect primes into the results array */
    collect_primes();

    /* Step 5: Set the done flag to signal completion */
    *DONE_FLAG = DONE_VALUE;

    /* The startup code will loop forever after main returns */
}
