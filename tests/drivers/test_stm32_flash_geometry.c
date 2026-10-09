/** Physical geometry regression independent of driver/vendor registers. */
#include "flash_geometry.h"
#include "../../soc/gd32f470zg/flash_geometry.h"
#include <assert.h>
#include <stdio.h>
static const uint32_t sizes[]={16384,16384,16384,16384,65536,
    131072,131072,131072,131072,131072,131072,131072};
int main(void) {
    nx_flash_geometry_t geometry;
    nx_flash_block_t block;
    uint32_t first,count;
    for (unsigned density=0;density<2;++density) {
        uint32_t bytes=density ? 1024U*1024U : 512U*1024U;
        assert(nx_stm32f407_flash_geometry(bytes,&geometry));
        assert(geometry.base_address==0x08000000U && geometry.size_bytes==bytes);
        assert(geometry.block_count==(density ? 12U : 8U) && geometry.program_alignment==4);
        uint32_t offset=0;
        for (uint32_t index=0;index<geometry.block_count;++index) {
            for (uint32_t i=0;i<sizes[index];++i) {
                assert(nx_stm32f407_flash_block(&geometry,offset+i,&block));
                assert(block.offset==offset && block.size==sizes[index] && block.index==index);
            }
            assert(nx_stm32f407_flash_erase_sector(&geometry,offset,sizes[index],&first,&count));
            assert(first==index && count==1);
            assert(!nx_stm32f407_flash_erase_sector(&geometry,offset+1,sizes[index]-1,&first,&count));
            assert(!nx_stm32f407_flash_erase_sector(&geometry,offset,sizes[index]-1,&first,&count));
            offset+=sizes[index];
        }
        assert(offset==bytes);
        assert(nx_stm32f407_flash_erase_sector(&geometry,0,bytes,&first,&count));
        assert(first==0 && count==geometry.block_count);
        assert(nx_stm32f407_flash_erase_sector(&geometry,0,81920,&first,&count)==false);
        assert(nx_stm32f407_flash_range(&geometry,bytes,0));
        assert(!nx_stm32f407_flash_range(&geometry,UINT32_MAX,1));
        assert(!nx_stm32f407_flash_range(&geometry,1,SIZE_MAX));
        assert(!nx_stm32f407_flash_block(&geometry,bytes,&block));
    }
    assert(!nx_stm32f407_flash_geometry(256U*1024U,&geometry));
    assert(!nx_stm32f407_flash_geometry(2U*1024U*1024U,&geometry));
    assert(!nx_stm32f407_flash_geometry(512U*1024U,NULL));
    assert(nx_gd32f470_flash_geometry(1048576,&geometry));
    assert(geometry.block_count==256 && geometry.program_alignment==2);
    for (uint32_t i=0;i<geometry.size_bytes;++i) {
        assert(nx_gd32f470_flash_block(&geometry,i,&block));
        assert(block.offset==i/4096*4096 && block.size==4096 && block.index==i/4096);
    }
    assert(nx_gd32f470_flash_range(&geometry,1048576,0));
    assert(!nx_gd32f470_flash_range(&geometry,UINT32_MAX,1));
    assert(!nx_gd32f470_flash_range(&geometry,0,SIZE_MAX));
    assert(!nx_gd32f470_flash_block(&geometry,1048576,&block));
    assert(!nx_gd32f470_flash_geometry(524288,&geometry));
    puts("Full STM32 xE/xG non-uniform sectors and GD32 F470 4KiB page geometry passed");
    return 0;
}
