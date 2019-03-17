/* The MIT License (MIT)

Copyright (c) 2015 Gabriel Corona

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <assert.h>
#include <stdlib.h>

#include <elf.h>
#include <arpa/inet.h>

#include "doltool.h"

#define ALIGN 0

static void init_load_text_phdr(const Dol_Hdr *dhdr, int i, Elf32_Phdr *phdr, struct Elf *elf)
{
  phdr->p_type    = htonl(PT_LOAD);
  phdr->p_offset  = htonl( ntohl(dhdr->text_offset[i]) + elf->dol_offset );
  phdr->p_vaddr   = dhdr->text_address[i];
  phdr->p_paddr   = dhdr->text_address[i];
  phdr->p_filesz  = dhdr->text_size[i];
  phdr->p_memsz   = dhdr->text_size[i];
  phdr->p_flags   = htonl(PF_X | PF_R);
  phdr->p_align   = htonl(ALIGN);
}

static void init_load_data_phdr(const Dol_Hdr *dhdr, int i, Elf32_Phdr *phdr, struct Elf *elf)
{
  phdr->p_type    = htonl(PT_LOAD);
  phdr->p_offset  = htonl( ntohl(dhdr->data_offset[i]) + elf->dol_offset );
  phdr->p_vaddr   = dhdr->data_address[i];
  phdr->p_paddr   = dhdr->data_address[i];
  phdr->p_filesz  = dhdr->data_size[i];
  phdr->p_memsz   = dhdr->data_size[i];
  phdr->p_flags   = htonl(PF_R | PF_W);
  phdr->p_align   = htonl(ALIGN);
}

static void init_load_bss_phdr(Elf32_Phdr *phdr, Elf32_Addr addr, Elf32_Word size)
{
  phdr->p_type    = htonl(PT_LOAD);
  phdr->p_offset  = 0;
  phdr->p_vaddr   = htonl(addr);
  phdr->p_paddr   = htonl(addr);
  phdr->p_filesz  = 0;
  phdr->p_memsz   = htonl(size);
  phdr->p_flags   = htonl(PF_R | PF_W);
  phdr->p_align   = htonl(ALIGN);
}

void create_phdrs(Dol_Hdr *dhdr, struct Elf *elf, int use_bss_fix)
{
  elf->phdrs = malloc(sizeof(Elf32_Phdr) * elf->phnum);

  size_t phindex = 0;
  size_t data_count = 0;

  for (int i=0; i != DOL_TEXT_COUNT; ++i)
    if (dhdr->text_size[i]) {
      init_load_text_phdr(dhdr, i, elf->phdrs + phindex, elf);
      ++phindex;
    }

  for (int i=0; i != DOL_DATA_COUNT; ++i)
    if (dhdr->data_size[i]) {
      init_load_data_phdr(dhdr, i, elf->phdrs + phindex, elf);
      ++phindex;
      ++data_count;
    }
  if (dhdr->bss_size) {
    if (use_bss_fix) {
      Elf32_Addr bss_address = ntohl(dhdr->bss_address);
      Elf32_Word bss_size = ntohl(dhdr->bss_size);
      Elf32_Addr sdata_address = ntohl(dhdr->data_address[data_count - 2]);
      Elf32_Word sdata_size = ntohl(dhdr->data_size[data_count - 2]);
      Elf32_Addr sdata2_address = ntohl(dhdr->data_address[data_count - 1]);
      Elf32_Word sdata2_size = ntohl(dhdr->data_size[data_count - 1]);

      Elf32_Addr bss_end = bss_address + bss_size;
      Elf32_Word actual_bss_size = sdata_address - bss_address;
      init_load_bss_phdr(elf->phdrs + phindex, bss_address, actual_bss_size);
      ++phindex;

      Elf32_Addr sbss_addr = sdata_address + sdata_size;
      Elf32_Word sbss_size = sdata2_address - sbss_addr;
      init_load_bss_phdr(elf->phdrs + phindex, sbss_addr, sbss_size);
      ++phindex;

      Elf32_Addr sbss2_addr = sdata2_address + sdata2_size;
      Elf32_Word sbss2_size = bss_end - sbss2_addr;
      init_load_bss_phdr(elf->phdrs + phindex, sbss2_addr, sbss2_size);
      ++phindex;
    } else {
      init_load_bss_phdr(elf->phdrs + phindex, ntohl(dhdr->bss_address), ntohl(dhdr->bss_size));
      ++phindex;
    }
  }
  assert(phindex == elf->phnum);
}
