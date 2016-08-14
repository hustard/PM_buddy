/*
 *
 *
 */

#include <linux/pm-buddy.h>

void pmbuddy_journal_init(struct pm_log *log_block){
}

int pmbuddy_add_logentry(struct page *dest, int zone, int order, int migratetype){
	return 0;
}

int pmbuddy_del_logentry(struct page *dest){
	return 0;
}

int pmbuddy_commit_alloc(struct page *dest){
	return 0;
}

int pmbuddy_gc(struct pm_log *log_block){
	return 0;
}

