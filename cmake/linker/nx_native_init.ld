/* Native ELF init callbacks use the same ordered section contract as MCU. */
SECTIONS
{
    .nx_init_fn :
    {
        . = ALIGN(8);
        __nx_init_fn_start = .;
        KEEP(*(SORT_BY_NAME(.nx_init_fn.*)))
        __nx_init_fn_end = .;
    }
}
INSERT AFTER .data;
