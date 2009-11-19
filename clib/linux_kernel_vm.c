/*
  Copyright (c) 2001, 2002, 2003, 2005 Eliot Dresselhaus

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* Kernel VM alloc/free code. */

#include <clib/mem.h>
#include <clib/vec.h>
#include <clib/vm_linux_kernel.h>

#include <linux/sched.h>	/* for CLONE_FS */
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/page.h>
#include <asm/tlbflush.h>

static void free_area_pte (pmd_t *pmd,
			   unsigned long address,
			   unsigned long size)
{
  unsigned long end;
  pte_t *pte;

  if (pmd_none (*pmd))
    return;
  if (pmd_bad (*pmd))
    {
      pmd_ERROR (*pmd);
      pmd_clear (pmd);
      return;
    }

  pte = pte_offset_kernel (pmd, address);
  address &= ~PMD_MASK;
  end = address + size;
  if (end > PMD_SIZE)
    end = PMD_SIZE;

  do {
    pte_t page;
    page = ptep_get_and_clear (pte);
    address += PAGE_SIZE;
    pte++;

    if (pte_none (page))
      continue;

    if (pte_present (page))
      {
	struct page *ptpage = pte_page (page);
	if (!PageReserved (ptpage))
	  __free_page (ptpage);
	continue;
      }

    printk (KERN_CRIT "Whee.. Swapped out page in kernel page table\n");
  } while (address < end);
}

static void free_area_pmd (pud_t * pud,
			   unsigned long address,
			   unsigned long size)
{
  unsigned long end;
  pmd_t *pmd;

  if (pud_none (*pud))
    return;
  if (pud_bad (*pud))
    {
      pud_ERROR (*pud);
      pud_clear (pud);
      return;
    }

  pmd = pmd_offset (pud, address);
  address &= ~PUD_MASK;
  end = address + size;
  if (end > PUD_SIZE)
    end = PUD_SIZE;

  do {
    free_area_pte (pmd, address, end - address);
    address = (address + PMD_SIZE) & PMD_MASK;
    pmd++;
  } while (address < end);
}

static void free_area_pud (pgd_t * pgd, unsigned long address,
			   unsigned long size)
{
  pud_t *pud;
  unsigned long end;

  if (pgd_none (*pgd))
    return;
  if (pgd_bad (*pgd))
    {
      pgd_ERROR (*pgd);
      pgd_clear (pgd);
      return;
    }

  pud = pud_offset (pgd, address);
  address &= ~PGDIR_MASK;
  end = address + size;
  if (end > PGDIR_SIZE)
    end = PGDIR_SIZE;

  do {
    free_area_pmd (pud, address, end - address);
    address = (address + PUD_SIZE) & PUD_MASK;
    pud++;
  } while (address && (address < end));
}

void clib_vmfree_area_pages (uword address, uword size)
{
  unsigned long end = address + size;
  unsigned long next;
  pgd_t *pgd;
  int i;

  pgd = pgd_offset_k (address);
  flush_cache_vunmap (address, end);
  for (i = pgd_index (address); i <= pgd_index (end-1); i++)
    {
      next = (address + PGDIR_SIZE) & PGDIR_MASK;
      if (next <= address || next > end)
	next = end;
      free_area_pud (pgd, address, next - address);
      address = next;
      pgd++;
    }
  flush_tlb_kernel_range ((unsigned long) area->addr, end);
}

static int alloc_area_pte (pte_t * pte,
			   unsigned long address,
			   unsigned long size,
			   int gfp_mask,
			   pgprot_t prot)
{
  unsigned long end;

  address &= ~PMD_MASK;
  end = address + size;
  if (end > PMD_SIZE)
    end = PMD_SIZE;

  do {
    struct page *page;
    WARN_ON (!pte_none (*pte));
    page = alloc_page (gfp_mask);
    if (!page)
      return -ENOMEM;

    set_pte (pte, mk_pte (page, prot));
    address += PAGE_SIZE;
    pte++;
  } while (address < end);
  return 0;
}

static int alloc_area_pmd (pmd_t * pmd,
			   unsigned long address,
			   unsigned long size,
			   int gfp_mask,
			   pgprot_t prot)
{
  unsigned long base, end;

  base = address & PUD_MASK;
  address &= ~PUD_MASK;
  end = address + size;
  if (end > PUD_SIZE)
    end = PUD_SIZE;

  do {
    pte_t * pte = pte_alloc_kernel (&init_mm, pmd, base + address);
    if (!pte)
      return -ENOMEM;
    if (alloc_area_pte (pte, address, end - address, gfp_mask, prot))
      return -ENOMEM;
    address = (address + PMD_SIZE) & PMD_MASK;
    pmd++;
  } while (address < end);

  return 0;
}

static int alloc_area_pud (pud_t * pud,
			   unsigned long address,
			   unsigned long end,
			   int gfp_mask,
			   pgprot_t prot)
{
  do {
    pmd_t *pmd = pmd_alloc (&init_mm, pud, address);
    if (!pmd)
      return -ENOMEM;
    if (alloc_area_pmd (pmd, address, end - address, gfp_mask, prot))
      return -ENOMEM;
    address = (address + PUD_SIZE) & PUD_MASK;
    pud++;
  } while (address && address < end);

  return 0;
}

int clib_vmalloc_area_pages (uword address,
			     uword size,
			     int gfp_mask,
			     pgprot_t prot)
{
  unsigned long end = address + (size-PAGE_SIZE);
  unsigned long next;
  pgd_t *pgd;
  int err = 0;
  int i;

  pgd = pgd_offset_k (address);
  spin_lock (&init_mm.page_table_lock);
  for (i = pgd_index (address); i <= pgd_index (end-1); i++)
    {
      pud_t *pud = pud_alloc (&init_mm, pgd, address);
      if (!pud)
	{
	  err = -ENOMEM;
	  break;
	}
      next = (address + PGDIR_SIZE) & PGDIR_MASK;
      if (next < address || next > end)
	next = end;
      if (alloc_area_pud (pud, address, next, gfp_mask, prot))
	{
	  err = -ENOMEM;
	  break;
	}

      address = next;
      pgd++;
    }

  spin_unlock (&init_mm.page_table_lock);
  flush_cache_vmap ((unsigned long) area->addr, end);
  return err;
}
