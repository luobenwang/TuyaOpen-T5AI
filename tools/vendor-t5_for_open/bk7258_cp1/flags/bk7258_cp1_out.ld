       
       
       
       
       
       
       
       
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
__MSP_STACK_SIZE = (4 << 10);
__MIN_HEAP_SIZE = (32 << 10);
__MPU_PROTECT_SIZE = 0x0;
__SWAP_SIZE = (2048);
__PRIMARY_APP_RAM_BASE = (0x28000000 + (0));
__BT_SRAM_BASE = __PRIMARY_APP_RAM_BASE + 0x40000;
__BT_SRAM_SIZE = 0;
__CPU1_APP_RAM_BASE = __BT_SRAM_BASE + __BT_SRAM_SIZE;
__CPU1_APP_RAM_SIZE = 0x60000 - __BT_SRAM_SIZE;
MEMORY
{
  FLASH (rx) : ORIGIN = ((0x02000000 + (0)) + 0x110000), LENGTH = 0x320000
  RAM (rwx) : ORIGIN = __CPU1_APP_RAM_BASE, LENGTH = __CPU1_APP_RAM_SIZE
  ITCM (rwx) : ORIGIN = (0x00000000 + (0)) + __MPU_PROTECT_SIZE, LENGTH = 0x4000 - __MPU_PROTECT_SIZE
  DTCM (rwx) : ORIGIN = (0x20000000 + (0)), LENGTH = 0x4000
  SWAP (rwx) : ORIGIN = __PRIMARY_APP_RAM_BASE + 0x40000 - __SWAP_SIZE, LENGTH = __SWAP_SIZE
  PSRAM (rwx) : ORIGIN = (0x60000000 + (0)), LENGTH = 0x4000000
}
ENTRY(Reset_Handler)
SECTIONS
{
    ASSERT((. == ALIGN(512)), "vector table address align fault.")
    .vectors :
    {
        __vector_table = .;
        KEEP(*(.vectors))
        . = ALIGN(128);
    } > FLASH
    .gnu.sgstubs ALIGN(32) : ALIGN(32)
    {
        *(.gnu.sgstubs*)
        . = ALIGN(32);
    } > FLASH
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        . = ALIGN(4);
        __devconfig_start = .;
        *(".devconfig.*")
        KEEP(*(SORT_BY_NAME(".devconfig*")))
        __devconfig_end = .;
        . = ALIGN(4);
        __apps_start = .;
        KEEP (*(.apps_data))
        __apps_end = .;
        _etext = .;
        . = ALIGN(4);
    } > FLASH
    .a_device_null :
    {
      KEEP(*(.a_deviceobj_null))
      . = ALIGN(4);
    } > FLASH
    .a_devices :
    {
      __device_start = .;
      KEEP(*(.a_deviceobj_*))
      . = ALIGN(4);
      __device_end = .;
    } > FLASH
    .a_init_entries :
    {
      __a_init_start = .;
      KEEP(*(.a_init_entry_*))
      . = ALIGN(4);
      __a_init_end = .;
    } > FLASH
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
    } > FLASH
    __exidx_end = .;
    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;
        LONG (__etext)
        LONG (__iram_start__)
        LONG ((__data_end__ - __iram_start__) / 4)
        LONG (__etext2)
        LONG (__data2_start__)
        LONG ((__data2_end__ - __data2_start__) / 4)
        LONG (__dtcm_data)
        LONG (__dtcm_start__)
        LONG ((__dtcm_end__ - __dtcm_start__) / 4)
        LONG (__itcm_text)
        LONG (__itcm_start__)
        LONG ((__itcm_end__ - __itcm_start__) / 4)
        . = ALIGN(4);
        __copy_table_end__ = .;
    } > FLASH
    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG (_bss_start)
        LONG ((_bss_end - _bss_start) / 4)
        LONG (_heap_start)
        LONG ((_heap_end - _heap_start) / 4)
        LONG (__bss2_start__)
        LONG ((__bss2_end__ - __bss2_start__) / 4)
        . = ALIGN(4);
        __zero_table_end__ = .;
    } > FLASH
    .itcm :
    {
        . = ALIGN(4);
        PROVIDE(__itcm_text = LOADADDR(.itcm));
        __itcm_start__ = .;
        KEEP(*(.null_trap_handler))
        *(.itcm)
        *(.itcm_section*)
        *(.itcm_sec_code*)
        *(.interrupt)
        . = ALIGN(4);
        __itcm_end__ = .;
    } > ITCM AT > FLASH
    .dtcm :
    {
        . = ALIGN(4);
        PROVIDE(__dtcm_data = LOADADDR(.dtcm));
        __dtcm_start__ = .;
        *(.dtcm)
        *(.dtcm_sec_data*)
        . = ALIGN(4);
        __dtcm_end__ = .;
    } > DTCM AT > FLASH
    .data :
    {
        . = ALIGN(512);
        PROVIDE(__etext = LOADADDR(.data));
        PROVIDE(__iram_flash_begin = LOADADDR(.data));
        __iram_start__ = .;
        . = ALIGN(512);
        __vector_iram_table = .;
        KEEP(*(.vectors_iram))
        . = ALIGN(128);
        *(.iram)
        __iram_end__ = .;
        . = ALIGN(4);
        __data_start__ = .;
        *(.data)
        *(".data.*")
        *(.sdata)
        *(.gnu.linkonce.d*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > FLASH
    _data_flash_begin = LOADADDR(.data);
    _data_ram_begin = ADDR(.data);
    _data_ram_end = .;
    s_rom_end = LOADADDR(.data) + SIZEOF(.data);
    .uninitialized (NOLOAD):
    {
        . = ALIGN(32);
        __uninitialized_start = .;
        *(.uninitialized)
        *(".uninitialized.*")
        KEEP(*(.keep.uninitialized))
        . = ALIGN(32);
        __uninitialized_end = .;
    } > RAM
    .bss (NOLOAD):
    {
        . = ALIGN(32);
        PROVIDE(_bt_data_lmastart = LOADADDR(.sram_bt));
        _bt_data_start = .;
        KEEP(*(.bt_spec_data ))
        KEEP(*(.ble_bss_data ))
        _bt_data_end = .;
        . = ALIGN(4);
        _bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM
    bss_size = _bss_end - _bss_start;
    .heap (COPY) :
    {
        . = ALIGN(8);
        _heap_start = .;
        . = . + (ORIGIN(RAM) + LENGTH(RAM) - __MSP_STACK_SIZE - _heap_start - 8);
        . = ALIGN(8);
        _heap_end = .;
    } > RAM
    .stack (ORIGIN(RAM) + LENGTH(RAM) - __MSP_STACK_SIZE) (COPY) :
    {
        . = ALIGN(8);
        _sstack = .;
        __StackLimit = .;
        . = . + __MSP_STACK_SIZE;
        . = ALIGN(8);
        __StackTop = .;
        _estack = .;
    } > RAM
    .swap ORIGIN(SWAP):
    {
        . = ALIGN(8);
        _swap_start = .;
        * (.swap_data)
        * (.swap_data*)
        . = ALIGN(4);
        _swap_end = .;
    } > SWAP AT > FLASH
    .video.cache.data :
    {
        . = ALIGN(4);
        PROVIDE(__etext2 = LOADADDR(.video.cache.data));
        __data2_start__ = .;
        *(.video_cache_data)
        *(.video_cache_data*)
        . = ALIGN(4);
        __data2_end__ = .;
    } > PSRAM AT > FLASH
    .video_cache_bss (NOLOAD):
    {
        . = ALIGN(4);
        __bss2_start__ = .;
        *(.video_cache_bss)
        *(.video_cache_bss*)
        . = ALIGN(4);
        __bss2_end__ = .;
    } > PSRAM
    __MSPTop = ORIGIN(RAM) + LENGTH(RAM);
    __MSPLimit = __MSPTop - __MSP_STACK_SIZE;
    ASSERT((s_rom_end < ORIGIN(FLASH) + LENGTH(FLASH)), "ROM overflow!!!")
    ASSERT(((ORIGIN(RAM) + LENGTH(RAM)) > (_heap_start + __MSP_STACK_SIZE + 8)), "Stack overflowed with bss")
    ASSERT(((_heap_end - _heap_start) >= __MIN_HEAP_SIZE), "Heap smaller than minimize size 32K!!!")
}
