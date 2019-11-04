#include "vm/frame.h"
#include <stdio.h>
#include <bitmap.h>
#include <round.h>
#include "threads/init.h"
#include "threads/palloc.h"
#include "threads/loader.h"
#include "threads/vaddr.h"

//static uint32_t* frame_base;
struct bitmap* frame_table;

void 
frame_init(void)
{
	/* Maximum number of frame */
	size_t max_page_cnt = (ptov(init_ram_pages * PGSIZE) - ptov(1024 * 1024)) / PGSIZE - 2; 

	/* Initialize bitmap into frame table, 
	   to be able to represent all physical frames */
	size_t bm_page_cnt = DIV_ROUND_UP(bitmap_buf_size(max_page_cnt), PGSIZE);

	frame_table = palloc_get_multiple(PAL_USER | PAL_ASSERT | PAL_ZERO, 
																	 bm_page_cnt);
	frame_table = bitmap_create_in_buf(max_page_cnt, frame_table, 
																		 bm_page_cnt * PGSIZE);

	printf("frame table allocated: %d pages at %p\n", max_page_cnt, frame_table);
}
/*
void 
frame_check

void 
frame_alloc(uint32_t paddr)
{
	bitmap_mark(frame_table, paddr);
}

void 
frame_released(
*/
