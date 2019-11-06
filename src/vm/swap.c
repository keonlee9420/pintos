#include "vm/swap.h"
#include "threads/synch.h"

static struct list swap_list;
static struct lock swap_lock;

void 
swap_init(void)
{
	list_init(&swap_list);
	lock_init(&swap_lock);
}
