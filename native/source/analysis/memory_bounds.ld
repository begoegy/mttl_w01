/* Offline extra linker constraints. No image generation or boot validation. */
__mttl_newlib_start = ALIGN(__tcm_bss_end__, 8);
__mttl_newlib_limit = 0x20000000;
__mttl_msp_limit = 0x1006F000;
__mttl_msp_top = 0x10070000;
ASSERT(__mttl_newlib_start <= __mttl_newlib_limit, "TCM allocator has no space")
ASSERT(__HeapLimit <= __mttl_msp_limit, "application overlaps reserved MSP budget")
