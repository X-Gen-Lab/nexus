#include "flash_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    nx_stm32f407_flash_geometry_t geometry;
    uint32_t first,count;
    for(unsigned i=0;i<2;i++) {
        uint32_t bytes=i ? 1024U*1024U : 512U*1024U;
        assert(nx_stm32f407_flash_geometry(bytes,&geometry));
        assert(geometry.partition_base == (i ? 0x080C0000U : 0x08040000U));
        assert(geometry.partition_base+geometry.partition_bytes == 0x08000000U+bytes);
        assert(nx_stm32f407_flash_erase_sector(&geometry,0,128U*1024U,&first,&count));
        assert(first == (i ? 10U : 6U) && count==1);
        assert(nx_stm32f407_flash_erase_sector(&geometry,128U*1024U,128U*1024U,&first,&count));
        assert(first == (i ? 11U : 7U) && count==1);
        assert(nx_stm32f407_flash_erase_sector(&geometry,0,256U*1024U,&first,&count) && count==2);
        assert(!nx_stm32f407_flash_erase_sector(&geometry,256U*1024U,128U*1024U,&first,&count));
        assert(!nx_stm32f407_flash_erase_sector(&geometry,1,128U*1024U,&first,&count));
        assert(!nx_stm32f407_flash_erase_sector(&geometry,0,1,&first,&count));
        assert(nx_stm32f407_flash_range(&geometry,geometry.partition_bytes,0));
        assert(!nx_stm32f407_flash_range(&geometry,SIZE_MAX,1));
        assert(!nx_stm32f407_flash_range(&geometry,1,SIZE_MAX));
    }
    assert(!nx_stm32f407_flash_geometry(256U*1024U,&geometry));
    assert(!nx_stm32f407_flash_geometry(2U*1024U*1024U,&geometry));
    assert(!nx_stm32f407_flash_geometry(512U*1024U,NULL));
    puts("STM32F407 xE/xG physical partition and erase geometry passed");
    return 0;
}
