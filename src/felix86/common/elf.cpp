#include <vector>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/resource.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"

// Not a full ELF implementation, but one that suits our needs as a loader of
// both the executable and the dynamic linker, and one that only supports x86_64
// little-endian

#define PAGE_START(x) ((x) & ~(uintptr_t)(4095))
#define PAGE_OFFSET(x) ((x) & 4095)
#define PAGE_ALIGN(x) (((x) + 4095) & ~(uintptr_t)(4095))

Elf::Elf(bool is_interpreter) : is_interpreter(is_interpreter) {}

Elf::~Elf() {
    if (program) {
        munmap(program, 0);
    }

    if (stack_base) {
        munmap(stack_base, 0);
    }

    if (stack_pointer) {
        munmap(stack_pointer, 0);
    }

    if (brk_base) {
        munmap(brk_base, 0);
    }
}

void Elf::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        WARN("File %s does not exist", path.c_str());
        return;
    }

    if (!std::filesystem::is_regular_file(path)) {
        WARN("File %s is not a regular file", path.c_str());
        return;
    }

    std::vector<u8> shdrtable = {};
    u64 lowest_vaddr = 0xFFFFFFFFFFFFFFFF;
    u64 highest_vaddr = 0;
    u64 max_stack_size = 0;

    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
    }

    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    if (size < sizeof(Elf64_Ehdr)) {
        ERROR("File %s is too small to be an ELF file", path.c_str());
    }
    fseek(file, 0, SEEK_SET);

    Elf64_Ehdr ehdr;
    ssize_t result = fread(&ehdr, sizeof(Elf64_Ehdr), 1, file);
    if (result != 1) {
        ERROR("Failed to read ELF header from file %s", path.c_str());
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        ERROR("File %s is not an ELF file", path.c_str());
    }

    if (ehdr.e_ident[4] != ELFCLASS64) {
        ERROR("File %s is not a 64-bit ELF file", path.c_str());
    }

    if (ehdr.e_ident[5] != ELFDATA2LSB) {
        ERROR("File %s is not a little-endian ELF file", path.c_str());
    }

    if (ehdr.e_ident[6] != 1 || ehdr.e_version != 1) {
        ERROR("File %s has an invalid version", path.c_str());
    }

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) {
        ERROR("File %s is not an executable or shared object", path.c_str());
    }

    if (ehdr.e_machine != EM_X86_64) {
        ERROR("File %s is not an x86_64 ELF file", path.c_str());
    }

    if (ehdr.e_entry == 0 && ehdr.e_type == ET_EXEC) {
        ERROR("File %s is an executable but has no entry point", path.c_str());
    }

    if (ehdr.e_phoff == 0) {
        ERROR("File %s has no program header table, thus has no loadable segments", path.c_str());
    }

    if (ehdr.e_phnum == 0xFFFF) {
        ERROR("If the number of program headers is greater than or equal to PN_XNUM "
              "(0xffff) "
              "this member has the value PN_XNUM (0xffff). The actual number of "
              "program header "
              "table entries is contained in the sh_info field of the section "
              "header at index 0");
    }

    entry = ehdr.e_entry;

    if (ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
        ERROR("File %s has an invalid program header size", path.c_str());
    }

    std::vector<Elf64_Phdr> phdrtable(ehdr.e_phnum);
    fseek(file, ehdr.e_phoff, SEEK_SET);
    result = fread(phdrtable.data(), sizeof(Elf64_Phdr), ehdr.e_phnum, file);
    if (result != ehdr.e_phnum) {
        ERROR("Failed to read program header table from file %s", path.c_str());
    }

    for (Elf64_Half i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr& phdr = phdrtable[i];
        switch (phdr.p_type) {
        case PT_INTERP: {
            std::string interpreter_str;
            interpreter_str.resize(phdr.p_filesz);
            fseek(file, phdr.p_offset, SEEK_SET);
            result = fread(interpreter_str.data(), 1, phdr.p_filesz, file);
            if (result != phdr.p_filesz) {
                ERROR("Failed to read interpreter from file %s", path.c_str());
            }

            interpreter = std::filesystem::path(interpreter_str);
            break;
        }
        case PT_GNU_STACK: {
            if (phdr.p_flags & PF_X) {
                WARN("Executable stack");
            }
            break;
        }
        case PT_LOAD: {
            if (phdr.p_filesz == 0) {
                break;
            }

            if (phdr.p_vaddr + phdr.p_memsz > highest_vaddr) {
                highest_vaddr = phdr.p_vaddr + phdr.p_memsz;
            }

            if (phdr.p_vaddr < lowest_vaddr) {
                lowest_vaddr = phdr.p_vaddr;
            }
            break;
        }
        }
    }

    // Allocate the stack first using host stack limits in case anyone wants to
    // configure it
    struct rlimit stack_limit = {0};
    if (getrlimit(RLIMIT_STACK, &stack_limit) == -1) {
        ERROR("Failed to get stack size limit");
    }

    u64 stack_size = stack_limit.rlim_cur;
    if (stack_size == RLIM_INFINITY) {
        stack_size = 8 * 1024 * 1024;
    }

    max_stack_size = stack_limit.rlim_max;
    if (max_stack_size == RLIM_INFINITY) {
        max_stack_size = 128 * 1024 * 1024;
    }

    u64 stack_hint = 0x7FFFFFFFF000 - max_stack_size;

    stack_base =
        (u8*)mmap((void*)stack_hint, max_stack_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN | MAP_NORESERVE, -1, 0);
    if (stack_base == MAP_FAILED) {
        ERROR("Failed to allocate stack for ELF file %s", path.c_str());
    }

    stack_pointer = (u8*)mmap(stack_base + max_stack_size - stack_size, stack_size, PROT_READ | PROT_WRITE,
                              MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN, -1, 0);
    if (stack_pointer == MAP_FAILED) {
        ERROR("Failed to allocate stack for ELF file %s", path.c_str());
    }
    VERBOSE("Allocated stack at %p", stack_base);
    stack_pointer += stack_size;
    VERBOSE("Stack pointer at %p", stack_pointer);

    u64 base_address = 0;
    if (ehdr.e_type == ET_DYN) {
        program = (u8*)mmap(NULL, highest_vaddr, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base_address = (u64)program;
        if (program == MAP_FAILED) {
            ERROR("Failed to allocate memory for ELF file %s", path.c_str());
        }
    } else {
        program = NULL;
        base_address = 0;
    }
    VERBOSE("Allocated program at %p", program);

    for (Elf64_Half i = 0; i < ehdr.e_phnum; i += 1) {
        Elf64_Phdr& phdr = phdrtable[i];
        switch (phdr.p_type) {
        case PT_LOAD: {
            if (phdr.p_filesz == 0) {
                ERROR("Loadable segment has no data in file %s", path.c_str());
                break;
            }

            u64 segment_base = base_address + PAGE_START(phdr.p_vaddr);
            u64 segment_size = phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr);
            u8* addr = (u8*)mmap((void*)segment_base, segment_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

            if (addr == MAP_FAILED) {
                ERROR("Failed to allocate memory for segment in file %s", path.c_str());
            } else {
                VERBOSE("Mapping segment with vaddr %p to %p-%p (file offset: %08lx)", (void*)phdr.p_vaddr, addr, addr + segment_size, phdr.p_offset);
                if (addr != (void*)segment_base) {
                    ERROR("Failed to allocate memory at requested address for segment in file %s", path.c_str());
                }
            }

            u8 prot = 0;
            if (phdr.p_flags & PF_R) {
                prot |= PROT_READ;
            }

            if (phdr.p_flags & PF_W) {
                prot |= PROT_WRITE;
            }

            if (phdr.p_flags & PF_X) {
                prot |= PROT_EXEC;
                executable_segments.push_back({addr, segment_size});
            }

            if (phdr.p_filesz > 0) {
                u64 offset = phdr.p_offset - PAGE_OFFSET(phdr.p_vaddr);
                u64 size = phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr);
                fseek(file, offset, SEEK_SET);
                result = fread(addr, 1, size, file);
                VERBOSE("Reading to range %p - %p from offset %08lx", addr, addr + size, offset);
                if (result != size) {
                    ERROR("Failed to read segment from file %s", path.c_str());
                }
            }

            mprotect(addr, segment_size, prot);

            if (phdr.p_memsz > phdr.p_filesz) {
                u64 bss_start = (u64)base_address + phdr.p_vaddr + phdr.p_filesz;
                u64 bss_page_start = PAGE_ALIGN(bss_start);
                u64 bss_page_end = PAGE_ALIGN((u64)base_address + phdr.p_vaddr + phdr.p_memsz);

                // Only clear padding bytes if the section is writable (why does FEX-Emu
                // do this?)
                if (phdr.p_flags & PF_W) {
                    memset((void*)bss_start, 0, bss_page_start - bss_start);
                }

                if (bss_page_start != bss_page_end) {
                    u8* bss = (u8*)mmap((void*)bss_page_start, bss_page_end - bss_page_start, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
                    if (bss == MAP_FAILED) {
                        ERROR("Failed to allocate memory for BSS in file %s", path.c_str());
                    }

                    VERBOSE("BSS segment at %p-%p", (void*)bss_page_start, (void*)bss_page_end);

                    memset(bss, 0, bss_page_end - bss_page_start);
                }
            }

            mprotect(addr, phdr.p_memsz, prot);
            break;
        }
        default: {
            break;
        }
        }
    }

    if (!is_interpreter) {
        const u64 brk_size = 8 * 1024 * 1024;
        brk_base = (u8*)mmap((void*)PAGE_ALIGN(highest_vaddr), brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (brk_base == MAP_FAILED) {
            ERROR("Failed to allocate memory for brk in file %s", path.c_str());
        }
        VERBOSE("BRK base at %p", brk_base);
    }

    phdr = (u8*)(base_address + lowest_vaddr + ehdr.e_phoff);
    phnum = ehdr.e_phnum;
    phent = ehdr.e_phentsize;

    fclose(file);

    ok = true;
}
