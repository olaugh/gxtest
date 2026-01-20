/**
 * gxtest - Stub implementations for unused Genesis Plus GX features
 *
 * These stubs allow the core to link without the full Sega CD, MegaSD,
 * and YX5200 audio chip implementations.
 */

#include <stdint.h>
#include <string.h>

/* Forward declarations for types used by stubs */
typedef struct { int dummy; } md_ntsc_t;
typedef struct { int dummy; } sms_ntsc_t;

/* NTSC filter objects (unused in headless mode) */
md_ntsc_t md_ntsc;
sms_ntsc_t sms_ntsc;

/* --------------------------------------------------------------------------
 * Sega CD stubs
 * -------------------------------------------------------------------------- */

void scd_init(void) {}
void scd_reset(int hard) { (void)hard; }
void scd_update(unsigned int cycles) { (void)cycles; }
void scd_end_frame(unsigned int cycles) { (void)cycles; }
int scd_68k_irq_ack(int level) { (void)level; return 0; }
int scd_context_save(uint8_t *state) { (void)state; return 0; }
int scd_context_load(uint8_t *state) { (void)state; return 0; }

/* CDD (CD Drive) stubs */
int cdd_load(const char *filename, char *header) {
    (void)filename; (void)header;
    return 0;  /* Fail to load */
}
void cdd_unload(void) {}
void cdd_update_audio(unsigned int samples) { (void)samples; }

/* PCM chip stubs */
void pcm_init(double clock, int samplerate) { (void)clock; (void)samplerate; }
void pcm_update(unsigned int samples) { (void)samples; }

/* DMA stubs */
void prg_ram_dma_w(unsigned int length) { (void)length; }
void word_ram_2M_dma_w(unsigned int length) { (void)length; }

/* --------------------------------------------------------------------------
 * MegaSD stubs (MegaSD is a flash cart with CD audio support)
 * -------------------------------------------------------------------------- */

void megasd_reset(void) {}
void megasd_rom_mapper_w(unsigned int address, unsigned int data) {
    (void)address; (void)data;
}
void megasd_enhanced_ssf2_mapper_w(unsigned int address, unsigned int data) {
    (void)address; (void)data;
}
int megasd_context_save(uint8_t *state) { (void)state; return 0; }
int megasd_context_load(uint8_t *state) { (void)state; return 0; }

/* --------------------------------------------------------------------------
 * YX5200 stubs (MP3 audio chip on some bootleg carts)
 * -------------------------------------------------------------------------- */

void yx5200_init(int samplerate) { (void)samplerate; }
void yx5200_reset(void) {}
void yx5200_write(unsigned int data) { (void)data; }
void yx5200_update(int *buffer, int length) { (void)buffer; (void)length; }
int yx5200_context_save(uint8_t *state) { (void)state; return 0; }
int yx5200_context_load(uint8_t *state) { (void)state; return 0; }

/* --------------------------------------------------------------------------
 * CDC (CD Controller) stubs
 * -------------------------------------------------------------------------- */

void cdc_dma_update(void) {}
unsigned int cdc_host_r(void) { return 0; }
void cdd_init(int samplerate) { (void)samplerate; }
