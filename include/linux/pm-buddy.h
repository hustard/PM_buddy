#ifndef __PM_BUDDY_H
#define __PM_BUDDY_H

#include <linux/mm_types.h>

/*
 *
 *
 *
 *
 */
//#ifdef CONFIG_LOG_PM_ALLOC

#define PMBUDDY_DEFAULT_JOURNAL_SIZE (4 << 24)

#define PMBUDDY_LOGENTRY_SIZE 16
#define PMBUDDY_LESIZE_SHIFT 4

struct pmlog_entry {
	struct page *addr;
	u32 zone_num;
	u8 page_order; 
	u8 commit;
}__aligned(2 * sizeof(unsigned long));
//8byte+4byte+1byte+1byte


struct pm_log {
	struct pmlog_entry entry[8];	
};

extern void pmbuddy_journal_init(struct pm_log *log_block);
extern int pmbuddy_add_logentry(struct page *dest, int zone, int order, int migratetype);
extern int pmbuddy_del_logentry(struct page *dest);
extern int pmbuddy_commit_alloc(struct page *dest);
extern int pmbuddy_gc(struct pm_log *log_block);

//#endif /* CONFIG_LOG_PM_ALLOC */
#endif /* __PM_BUDDY_H */
