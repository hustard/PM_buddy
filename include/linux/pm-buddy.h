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

//#define PMJOURNAL_SIZE 0x6400000UL
#define PMJOURNAL_SIZE_SHIFT 27
#define PMJOURNAL_SIZE (1UL << PMJOURNAL_SIZE_SHIFT)//128MB

#define PM_LESIZE_SHIFT 4
#define PM_LESIZE (1UL << PM_LESIZE_SHIFT)//16

#define PMLE_SHIFT (PAGE_SHIFT - PM_LESIZE_SHIFT) //12 - 4 = 8
#define PMLE_PER_PAGE (1UL << PM_LESIZE_SHIFT) // 256

#define NR_PMLE_SHIFT (PMJOURNAL_SIZE_SHIFT - PM_LESIZE_SHIFT)//27 - 4 = 23 
#define NR_PMBUDDY_ENTRYS (1UL << NR_PMLE_SHIFT)// 8388608(0x800000)

#define PMLE_PER_HUGEPAGE (HPAGE_SIZE / PM_LESIZE) //131072

#define NR_PMLE_BLOCKS (PMJOURNAL_SIZE / HPAGE_SIZE)

/* memmap is virtually contiguous.  */
#define pmlemap ((struct pmlog_entry *)PMLOG_START)
#define __idx_to_ple(idx)	(pmlemap + (idx))
#define __ple_to_idx(ple)	(unsigned long)((ple) - pmlemap)

#define idx_to_ple __idx_to_ple
#define ple_to_idx __ple_to_idx

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

void pmbuddy_init(void);

//#endif /* CONFIG_LOG_PM_ALLOC */
#endif /* __PM_BUDDY_H */
