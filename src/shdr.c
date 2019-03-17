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

static void init_null_shdr(Elf32_Shdr *shdr, struct Elf *elf)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, ""));
  shdr->sh_type  = htonl(SHT_NULL);
  shdr->sh_flags = 0;
  shdr->sh_addr  = 0;
  shdr->sh_offset = 0;
  shdr->sh_size = 0;
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}

static void init_text_shdr(Dol_Hdr *dhdr, int i, Elf32_Shdr *shdr, struct Elf *elf)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, text_sections[i]));
  shdr->sh_type  = htonl(SHT_PROGBITS);
  shdr->sh_flags = htonl(SHF_ALLOC | SHF_EXECINSTR);
  shdr->sh_addr  = dhdr->text_address[i];
  shdr->sh_offset = htonl(ntohl(dhdr->text_offset[i]) + elf->dol_offset);
  shdr->sh_size = dhdr->text_size[i];
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}

static void init_data_shdr(Dol_Hdr *dhdr, int i, Elf32_Shdr *shdr, struct Elf *elf)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, data_sections[i]));
  shdr->sh_type  = htonl(SHT_PROGBITS);
  shdr->sh_flags = htonl(SHF_ALLOC | SHF_WRITE);
  shdr->sh_addr  = dhdr->data_address[i];
  shdr->sh_offset = htonl(ntohl(dhdr->data_offset[i]) + elf->dol_offset);
  shdr->sh_size = dhdr->data_size[i];
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}


static void init_bss_shdr(Elf32_Shdr *shdr, struct Elf *elf, Elf32_Addr addr, Elf32_Word size, const char* name)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, name));
  shdr->sh_type  = htonl(SHT_NOBITS);
  shdr->sh_flags = htonl(SHF_ALLOC | SHF_WRITE);
  shdr->sh_addr  = htonl(addr);
  shdr->sh_offset = htonl(elf->dol_offset);
  shdr->sh_size = htonl(size);
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}

static void init_strtab_shdr(Elf32_Shdr *shdr, struct Elf *elf)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, ".shstrtab"));
  shdr->sh_type  = htonl(SHT_STRTAB);
  shdr->sh_flags = 0;
  shdr->sh_addr  = 0;
  shdr->sh_offset = htonl(elf->strtab_offset);
  shdr->sh_size = htonl(elf->strtab.used);
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}

static void init_dol_shdr(Elf32_Shdr *shdr, struct Elf *elf)
{
  shdr->sh_name  = htonl(strtab_index(&elf->strtab, ".dolhdr"));
  shdr->sh_type  = htonl(SHT_PROGBITS);
  shdr->sh_flags = 0;
  shdr->sh_addr  = 0;
  shdr->sh_offset = htonl(elf->dol_offset);
  shdr->sh_size = htonl(sizeof(Dol_Hdr));
  shdr->sh_link = 0;
  shdr->sh_info = 0;
  shdr->sh_addralign = 0;
  shdr->sh_entsize = 0;
}

void create_shdrs(Dol_Hdr *dhdr, struct Elf *elf, int use_bss_fix)
{
  elf->shdrs = malloc(sizeof(Elf32_Shdr) * elf->shnum);

  size_t shindex = 0;
  init_null_shdr(elf->shdrs + shindex, elf);
  ++shindex;

  init_strtab_shdr(elf->shdrs + shindex, elf);
  ++shindex;

  for (int i=0; i != DOL_TEXT_COUNT; ++i)
    if (dhdr->text_size[i]) {
      init_text_shdr(dhdr, i, elf->shdrs + shindex, elf);
      ++shindex;
    }

  size_t data_count = 0;
  for (int i=0; i != DOL_DATA_COUNT; ++i)
    if (dhdr->data_size[i]) {
      init_data_shdr(dhdr, i, elf->shdrs + shindex, elf);
      ++shindex;
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
      init_bss_shdr(elf->shdrs + shindex, elf, bss_address, actual_bss_size, ".bss");
      ++shindex;

      Elf32_Addr sbss_addr = sdata_address + sdata_size;
      Elf32_Word sbss_size = sdata2_address - sbss_addr;
      init_bss_shdr(elf->shdrs + shindex, elf, sbss_addr, sbss_size, ".sbss");
      ++shindex;

      Elf32_Addr sbss2_addr = sdata2_address + sdata2_size;
      Elf32_Word sbss2_size = bss_end - sbss2_addr;
      init_bss_shdr(elf->shdrs + shindex, elf, sbss2_addr, sbss2_size, ".sbss2");
      ++shindex;
    } else {
      init_bss_shdr(elf->shdrs + shindex, elf, ntohl(dhdr->bss_address), ntohl(dhdr->bss_size), ".bss");
      ++shindex;
    }
  }

  init_dol_shdr(elf->shdrs + shindex, elf);
  ++shindex;
}
