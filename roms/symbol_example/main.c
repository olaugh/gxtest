/**
 * Symbol Example ROM for gxtest
 *
 * Demonstrates symbol-based testing by exposing global variables
 * that can be extracted from the ELF and used in test assertions.
 *
 * This ROM simulates a simple game loop that:
 *   - Increments a score counter each frame
 *   - Tracks player lives
 *   - Sets a game_over flag when conditions are met
 *   - Maintains a game_state enum
 *
 * The test harness can inject values and verify state transitions.
 */

/* Bare-metal type definitions (no standard library) */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/* ==========================================================================
 * GLOBAL VARIABLES (these will appear in the ELF symbol table)
 * ==========================================================================
 * Note: We use 'volatile' to prevent the compiler from optimizing away
 * reads/writes, since the test harness will be poking at these externally.
 */

/* Player state */
volatile uint16_t player_score = 0;     /* Current score (increments by 10 each frame) */
volatile uint8_t  player_lives = 3;     /* Lives remaining */
volatile uint16_t player_x = 160;       /* X position (center of 320-wide screen) */
volatile uint16_t player_y = 200;       /* Y position */

/* Game state */
volatile uint8_t  game_state = 0;       /* 0=init, 1=playing, 2=paused, 3=game_over */
volatile uint8_t  game_over = 0;        /* Non-zero when game has ended */
volatile uint16_t frame_count = 0;      /* Frames elapsed */
volatile uint16_t level = 1;            /* Current level */

/* Enemy state (for testing array-like access) */
volatile uint16_t enemy_x = 50;
volatile uint16_t enemy_y = 50;
volatile uint8_t  enemy_active = 1;

/* Sentinel values for test synchronization */
volatile uint16_t init_complete = 0;    /* Set to 0xBEEF when init done */
volatile uint16_t done_flag = 0;        /* Set to 0xDEAD when game ends */

/* ==========================================================================
 * GAME STATE CONSTANTS
 * ========================================================================== */
#define STATE_INIT      0
#define STATE_PLAYING   1
#define STATE_PAUSED    2
#define STATE_GAME_OVER 3

#define INIT_SENTINEL   0xBEEF
#define DONE_SENTINEL   0xDEAD

#define SCORE_PER_FRAME 10
#define SCORE_FOR_LEVEL 1000
#define MAX_LEVEL       5

/* ==========================================================================
 * GAME LOGIC
 * ========================================================================== */

/**
 * Initialize game state
 */
static void game_init(void)
{
    player_score = 0;
    player_lives = 3;
    player_x = 160;
    player_y = 200;
    game_state = STATE_INIT;
    game_over = 0;
    frame_count = 0;
    level = 1;
    enemy_x = 50;
    enemy_y = 50;
    enemy_active = 1;
    done_flag = 0;

    /* Signal initialization complete */
    init_complete = INIT_SENTINEL;
    game_state = STATE_PLAYING;
}

/**
 * Update score and check for level advancement
 */
static void update_score(void)
{
    player_score += SCORE_PER_FRAME;

    /* Level up every 1000 points, capped at MAX_LEVEL */
    uint16_t expected_level = (player_score / SCORE_FOR_LEVEL) + 1;
    if (expected_level > MAX_LEVEL) {
        expected_level = MAX_LEVEL;
    }
    if (expected_level > level) {
        level = expected_level;
    }
}

/**
 * Simple enemy movement (bounces around)
 */
static void update_enemy(void)
{
    if (!enemy_active) {
        return;
    }

    /* Move enemy based on frame count */
    enemy_x = 50 + (frame_count % 200);
    enemy_y = 50 + ((frame_count / 2) % 150);
}

/**
 * Check for collision between player and enemy
 * Returns 1 if collision detected
 */
static int check_collision(void)
{
    if (!enemy_active) {
        return 0;
    }

    /* Simple bounding box collision (32x32 sprites assumed) */
    int dx = (int)player_x - (int)enemy_x;
    int dy = (int)player_y - (int)enemy_y;

    if (dx < 0) dx = -dx;  /* abs */
    if (dy < 0) dy = -dy;

    return (dx < 32 && dy < 32);
}

/**
 * Handle player taking damage
 */
static void player_hit(void)
{
    if (player_lives > 0) {
        player_lives--;
    }

    if (player_lives == 0) {
        game_over = 1;
        game_state = STATE_GAME_OVER;
        done_flag = DONE_SENTINEL;
    }

    /* Brief invincibility: deactivate enemy temporarily */
    enemy_active = 0;
}

/**
 * Main game loop iteration
 */
static void game_update(void)
{
    if (game_state != STATE_PLAYING) {
        return;
    }

    frame_count++;

    /* Update game objects */
    update_score();
    update_enemy();

    /* Reactivate enemy after some frames */
    if (!enemy_active && (frame_count % 60) == 0) {
        enemy_active = 1;
    }

    /* Check win condition: reach max level with high score */
    if (level >= MAX_LEVEL && player_score >= 5000) {
        game_over = 1;
        game_state = STATE_GAME_OVER;
        done_flag = DONE_SENTINEL;
    }

    /* Check collision (lose condition handled in player_hit) */
    if (check_collision()) {
        player_hit();
    }
}

/**
 * Main entry point
 */
void main(void)
{
    /* Initialize game */
    game_init();

    /* Run game loop until game over or max frames reached */
    while (!game_over && frame_count < 1000) {
        game_update();
    }

    /* Ensure done_flag is set if we exited due to frame limit */
    if (!done_flag) {
        done_flag = DONE_SENTINEL;
    }

    /* The startup code will loop forever after main returns */
}
