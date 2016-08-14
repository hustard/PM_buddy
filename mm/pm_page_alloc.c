/*
 *
 *
 */

#include <linux/pm-buddy.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

static void *pmlog_buf;
static void *pmlog_buf_end;
static long __meminitdata addr_start, addr_end;
static void __meminitdata *p_start, *p_end;
static int __meminitdata node_start;

void * __meminit pmlog_alloc_block(unsigned long size, int node)
{
	/* If the main allocator is up use that, fallback to bootmem. */

	if (slab_is_available()) {
		struct page *page;

		if (node_state(node, N_HIGH_MEMORY))
			page = alloc_pages_node(
				node, GFP_KERNEL | __GFP_ZERO | __GFP_REPEAT,
				get_order(size));
		else
			page = alloc_pages(
				GFP_KERNEL | __GFP_ZERO | __GFP_REPEAT,
				get_order(size));
		if (page)
			return page_address(page);
		return NULL;
	} else
		return memblock_virt_alloc_try_nid(size, size, __pa(MAX_PMMIGRATE_ADDRESS),
					    BOOTMEM_ALLOC_ACCESSIBLE, node);
//		return __earlyonly_bootmem_alloc(node, size, size,
//				__pa(MAX_PMMIGRATE_ADDRESS));
}

static void * __meminit pmlog_alloc_block_buf(unsigned long size, int node)
{
	void *ptr;
	
	if (!pmlog_buf)
		return pmlog_alloc_block(size, node);

	ptr = (void *)ALIGN((unsigned long)pmlog_buf, size);
	if (ptr + size > pmlog_buf_end)
		return pmlog_alloc_block(size, node);

	pmlog_buf = ptr + size;

	return ptr;
}

void __meminit pmlog_verify(pte_t *pte, int node,
				unsigned long start, unsigned long end)
{
	unsigned long pfn = pte_pfn(*pte);
	int actual_node = early_pfn_to_nid(pfn);

	if (node_distance(actual_node, node) > LOCAL_DISTANCE)
		printk(KERN_WARNING "[%lx-%lx] potential offnode "
			"page_structs\n", start, end - 1);
}

pte_t * __meminit pmlog_pte_populate(pmd_t *pmd, unsigned long addr, int node)
{
	pte_t *pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte)) {
		pte_t entry;
		void *p = pmlog_alloc_block_buf(PAGE_SIZE, node);
		if (!p)
			return NULL;
		entry = pfn_pte(__pa(p) >> PAGE_SHIFT, PAGE_KERNEL);
		set_pte_at(&init_mm, addr, pte, entry);
	}
	return pte;
}

pmd_t * __meminit pmlog_pmd_populate(pud_t *pud, unsigned long addr, int node)
{
	pmd_t *pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd)) {
		void *p = pmlog_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pmd_populate_kernel(&init_mm, pmd, p);
	}
	return pmd;
}

pud_t * __meminit pmlog_pud_populate(pgd_t *pgd, unsigned long addr, int node)
{
	pud_t *pud = pud_offset(pgd, addr);
	if (pud_none(*pud)) {
		void *p = pmlog_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pud_populate(&init_mm, pud, p);
	}
	return pud;
}

pgd_t * __meminit pmlog_pgd_populate(unsigned long addr, int node)
{
	pgd_t *pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd)) {
//		printk("hustard: hit here??\n");
		void *p = pmlog_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pgd_populate(&init_mm, pgd, p);
	//	printk("hustard: page populated??\n");
	}
	return pgd;
}

int __meminit pmlog_populate_basepages(unsigned long start,
					 unsigned long end, int node)
{
	unsigned long addr = start;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	for (; addr < end; addr += PAGE_SIZE) {
		pgd = pmlog_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;
		pud = pmlog_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;
		pmd = pmlog_pmd_populate(pud, addr, node);
		if (!pmd)
			return -ENOMEM;
		pte = pmlog_pte_populate(pmd, addr, node);
		if (!pte)
			return -ENOMEM;
		pmlog_verify(pte, node, addr, addr + PAGE_SIZE);
	}

	return 0;
}

static int __meminit pmlog_block_populate_hugepage(unsigned long start, unsigned long end, int node)
{
	unsigned long addr;
	unsigned long next;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	for (addr = start; addr < end; addr = next) {
		next = pmd_addr_end(addr, end);

		pgd = pmlog_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;

		pud = pmlog_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd)) {
			void *p;

			p = pmlog_alloc_block_buf(PMD_SIZE, node);
			printk("hustard: %016lx \n", p);
			//hustard p is ffffea000000~
			if (p) {
				pte_t entry;

				//hustard: set pmd entry, address in pte start with 607400000
				entry = pfn_pte(__pa(p) >> PAGE_SHIFT,
						PAGE_KERNEL_LARGE);
				set_pmd(pmd, __pmd(pte_val(entry)));

				printk("hustard: pte entry %016lx \n", entry);
				printk("hustard: __(p) %016lx \n", __pa(p));
				/* check to see if we have contiguous blocks */
				if (p_end != p || node_start != node) {
					if (p_start)
						pr_debug(" [%lx-%lx] PMD -> [%p-%p] on node %d\n",
						       addr_start, addr_end-1, p_start, p_end-1, node_start);
					addr_start = addr;
					node_start = node;
					p_start = p;
				}

				addr_end = addr + PMD_SIZE;
				p_end = p + PMD_SIZE;
				continue;
			}
		} else if (pmd_large(*pmd)) {
			pmlog_verify((pte_t *)pmd, node, addr, next);
			continue;
		}
		pr_warn_once("pmlog: falling back to regular page backing\n");
		if (pmlog_populate_basepages(addr, next, node))
			return -ENOMEM;
	}
	return 0;
}

struct pmlog_entry * __meminit pmlog_block_populate(unsigned long bnum, int nid)
{
	unsigned long start;
	unsigned long end;
	struct pmlog_entry* ple_block;
	int err;

	ple_block = idx_to_ple(bnum * PMLE_PER_HUGEPAGE);
	start = (unsigned long)ple_block;
	end = (unsigned long)(ple_block + PMLE_PER_HUGEPAGE);
	printk("\nmap ptr *%016lx, map start %016lx, end %016lx\n", ple_block, start, end);

	err = pmlog_block_populate_hugepage(start, end, 0);

	if (!err){
		sync_global_pgds(start, end - 1, 0);
	} else
		return NULL;

	return ple_block;
}

void __init pmlog_blocks_populate(struct page **pmlog_entry)
{
	unsigned long lenum;
	unsigned long bnum;
	unsigned long size = sizeof(struct pmlog_entry) * NR_PMBUDDY_ENTRYS;
	void *pmlog_buf_start;

	struct pmlog_entry *base_addr;

	size = ALIGN(size, PMD_SIZE);
//	pmlog_buf_start = __earlyonly_bootmem_alloc(0, size,
//			 PMD_SIZE, __pa(MAX_PMMIGRATE_ADDRESS));
  	pmlog_buf_start = memblock_virt_alloc_try_nid(size, PMD_SIZE, __pa(MAX_PMMIGRATE_ADDRESS),
					    BOOTMEM_ALLOC_ACCESSIBLE, 0);
	
	printk("\nPMBUDDY log area initialize start\n", size);
	printk("MAX_PMMIGRATE_ADDRESS %lx, __pa(%lx)\n", MAX_PMMIGRATE_ADDRESS, __pa(MAX_PMMIGRATE_ADDRESS));
	printk("pmlog_buf_start %lx, pa %lx, virt_to_phys %lx\n", pmlog_buf_start, __pa(pmlog_buf_start), virt_to_phys(pmlog_buf_start));
	printk("size %llx\n\n", size);

	if (pmlog_buf_start) {
		pmlog_buf = pmlog_buf_start;
		pmlog_buf_end = pmlog_buf_start + size;
	}

	for ( bnum = 0; bnum < NR_PMLE_BLOCKS; bnum++) {
		 pmlog_entry[bnum] = pmlog_block_populate(bnum, 0);
		 if (pmlog_entry[bnum])
			 continue;
		 printk(KERN_ERR "%s: pmlog area initialize failed.\n", __func__);
	}

//	for (lenum = 0; lenum < NR_PMBUDDY_ENTRYS; lenum++) {//each section map_map allocation
//		if (map_map[lenum])
//			continue;
//	}

	if (pmlog_buf_start) {
		/* need to free left buf */
		memblock_free_early(__pa(pmlog_buf),
				    pmlog_buf_end - pmlog_buf);
		pmlog_buf = NULL;
		pmlog_buf_end = NULL;
	}
}


void __init pmbuddy_init(void){

	int size;
	struct pmlog_entry **entry_node;
	struct pmlog_entry *base_entry;
	struct pmlog_entry *iter_entry;

	size = sizeof(struct pmlog_entry *) * NR_PMBUDDY_ENTRYS;
//	entry_node = memblock_virt_alloc(size, 0);
	entry_node = memblock_virt_alloc_try_nid(size, 0, __pa(MAX_PMMIGRATE_ADDRESS),
					    BOOTMEM_ALLOC_ACCESSIBLE,
					    NUMA_NO_NODE);
	if (!entry_node)
		panic("can not allocate pmbuddy log area\n");
//	alloc_usemap_and_memmap(sparse_early_mem_maps_alloc_node,
//							(void *)entry_node);

	pmlog_blocks_populate((void *)entry_node);

	base_entry = (struct pmlog_entry *) entry_node;
	for (lenum = 0; lenum < NR_PMBUDDY_ENTRYS; lenum++) {
		iter_entry = base_entry + 0;
		if (!entry)
			panic("can not allocate pmbuddy %lu log entry\n", lenum);
		else {
			iter_entry->addr = 0xFFFFFFFF;
			iter_entry->zone_num = 0xFFFFFFFF;
			iter_entry->page_order = 0xFF;
		}
	}

//	pmlog_populate_print_last();
	memblock_free_early(__pa(entry_node), size);
}


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

