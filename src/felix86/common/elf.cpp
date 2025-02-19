#include <vector>
#include <cxxabi.h>
#include <elf.h>
#include <linux/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include "felix86/common/debug.hpp"
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/thread.hpp"

// Not a full ELF implementation, but one that suits our needs as a loader of
// both the executable and the dynamic linker, and one that only supports x86_64
// little-endian

#define PAGE_START(x) ((x) & ~(uintptr_t)(4095))
#define PAGE_OFFSET(x) ((x) & 4095)
#define PAGE_ALIGN(x) (((x) + 4095) & ~(uintptr_t)(4095))

Elf::Elf(bool is_interpreter) : is_interpreter(is_interpreter) {}

Elf::~Elf() {
    if (stack_pointer) {
        munmap(stack_pointer, 0);
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

    u64 lowest_vaddr = 0xFFFFFFFFFFFFFFFF;
    u64 highest_vaddr = 0;

    FILE* file = fopen(path.c_str(), "rb");
    int fd = fileno(file);

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

    // TODO: this allocates it twice interpreter and executable, fix me.
    stack_pointer = (u8*)Threads::AllocateStack().first;

    u64 base_hint = is_interpreter ? g_interpreter_base_hint : g_executable_base_hint;

    for (Elf64_Half i = 0; i < ehdr.e_phnum; i += 1) {
        Elf64_Phdr& phdr = phdrtable[i];
        switch (phdr.p_type) {
        case PT_LOAD: {
            if (phdr.p_filesz == 0) {
                ERROR("Loadable segment has no data in file %s", path.c_str());
                break;
            }

            u64 segment_base = base_hint + PAGE_START(phdr.p_vaddr);
            u64 segment_size = phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr);

            u8 prot = 0;
            if (phdr.p_flags & PF_R) {
                prot |= PROT_READ;
            }

            if (phdr.p_flags & PF_W) {
                prot |= PROT_WRITE;
            }

            if (phdr.p_flags & PF_X) {
                prot |= PROT_EXEC;
            }

            void* addr = MAP_FAILED;

            u32 fixed = 0;
            if (is_interpreter && g_interpreter_base_hint) {
                fixed = MAP_FIXED_NOREPLACE;
            } else if (!is_interpreter && g_executable_base_hint) {
                fixed = MAP_FIXED_NOREPLACE;
            }

            if (phdr.p_memsz > phdr.p_filesz) {
                u64 bss_start = (u64)base_hint + phdr.p_vaddr + phdr.p_filesz;
                u64 bss_page_start = PAGE_ALIGN(bss_start);
                u64 bss_page_end = PAGE_ALIGN((u64)base_hint + phdr.p_vaddr + phdr.p_memsz);

                if (bss_page_start != bss_page_end) {
                    addr = mmap((void*)bss_page_start, bss_page_end - bss_page_start, PROT_READ | PROT_WRITE, MAP_PRIVATE | fixed | MAP_ANONYMOUS, -1,
                                0);
                    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, addr, bss_page_end - bss_page_start, "bss");
                    memset(addr, 0, bss_page_end - bss_page_start);
                    VERBOSE("BSS segment at %p-%p", (void*)bss_page_start, (void*)bss_page_end);

                    if (addr == MAP_FAILED) {
                        ERROR("Failed to allocate memory for BSS segment in file %s. Error: %s", path.c_str(), strerror(errno));
                    }
                }
            } else {
                addr = mmap((void*)segment_base, segment_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | fixed, fd, phdr.p_offset);

                if (addr == MAP_FAILED) {
                    ERROR("Failed to allocate memory for segment in file %s. Error: %s", path.c_str(), strerror(errno));
                } else if (addr != (void*)segment_base) {
                    ERROR("Failed to allocate memory at requested address for segment in file %s", path.c_str());
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
        g_current_brk = (u64)mmap((void*)PAGE_ALIGN(highest_vaddr), brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((void*)g_current_brk == MAP_FAILED) {
            ERROR("Failed to allocate memory for brk in file %s", path.c_str());
        }
        g_initial_brk = g_current_brk;
        g_current_brk_size = brk_size;
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_current_brk, brk_size, "brk");
        VERBOSE("BRK base at %p", (void*)g_current_brk);

        g_executable_start = base_hint + lowest_vaddr;
        g_executable_end = base_hint + highest_vaddr;
        program_base = (u8*)g_executable_start;
        MemoryMetadata::AddRegion("Executable", g_executable_start, g_executable_end);
        // LoadSymbols("Executable", path, (void*)g_executable_start);
    } else {
        g_interpreter_start = base_hint + lowest_vaddr;
        g_interpreter_end = base_hint + highest_vaddr;
        MemoryMetadata::AddInterpreterRegion(g_interpreter_start, g_interpreter_end);
        // LoadSymbols("Interpreter", path, (void*)g_interpreter_start);
    }

    phdr = (u8*)(base_hint + lowest_vaddr + ehdr.e_phoff);
    phnum = ehdr.e_phnum;
    phent = ehdr.e_phentsize;

    fclose(file);

    ok = true;
}

// void Elf::LoadSymbols(const std::string& name, const std::filesystem::path& path, void* base) {
//     FILE* file = fopen(path.c_str(), "rb");
//     if (!file) {
//         ERROR("Failed to open file %s", path.c_str());
//     }

//     fseek(file, 0, SEEK_END);
//     u64 size = ftell(file);
//     if (size < sizeof(Elf64_Ehdr)) {
//         fclose(file);
//         return;
//     }
//     fseek(file, 0, SEEK_SET);

//     Elf64_Ehdr ehdr;
//     size_t result = fread(&ehdr, sizeof(Elf64_Ehdr), 1, file);
//     if (result != 1) {
//         ERROR("Failed to read ELF header from file %s", path.c_str());
//     }

//     if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
//         fclose(file); // silently return, not an ELF file
//         return;
//     }

//     if (ehdr.e_shnum == 0) {
//         fclose(file); // no sections, return
//         return;
//     }

//     std::vector<Elf64_Shdr> shdrtable(ehdr.e_shnum);
//     fseek(file, ehdr.e_shoff, SEEK_SET);
//     result = fread(shdrtable.data(), sizeof(Elf64_Shdr), ehdr.e_shnum, file);
//     if (result != ehdr.e_shnum) {
//         ERROR("Failed to read section header table from file %s", path.c_str());
//     }

//     Elf64_Shdr shstrtab = shdrtable[ehdr.e_shstrndx];
//     std::vector<char> shstrtab_data(shstrtab.sh_size);
//     fseek(file, shstrtab.sh_offset, SEEK_SET);
//     result = fread(shstrtab_data.data(), shstrtab.sh_size, 1, file);
//     if (result != 1) {
//         ERROR("Failed to read section header string table from file %s", path.c_str());
//     }

//     Elf64_Shdr dynstr{};
//     for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
//         Elf64_Shdr& shdr = shdrtable[i];
//         if (shdr.sh_type == SHT_STRTAB && strcmp(&shstrtab_data[shdr.sh_name], ".dynstr") == 0) {
//             dynstr = shdr;
//             break;
//         }
//     }

//     if (dynstr.sh_type == SHT_STRTAB) {
//         std::vector<char> dynstr_data(dynstr.sh_size);
//         fseek(file, dynstr.sh_offset, SEEK_SET);
//         result = fread(dynstr_data.data(), dynstr.sh_size, 1, file);
//         if (result != 1) {
//             ERROR("Failed to read dynamic string table from file %s", path.c_str());
//         }

//         for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
//             Elf64_Shdr& shdr = shdrtable[i];
//             if (shdr.sh_type == SHT_DYNSYM) {
//                 std::vector<Elf64_Sym> dynsym(shdr.sh_size / shdr.sh_entsize);
//                 fseek(file, shdr.sh_offset, SEEK_SET);
//                 result = fread(dynsym.data(), shdr.sh_entsize, dynsym.size(), file);
//                 if (result != dynsym.size()) {
//                     ERROR("Failed to read dynamic symbol table from file %s", path.c_str());
//                 }

//                 std::string mangle_buffer;
//                 mangle_buffer.resize(4096);

//                 FELIX86_LOCK;
//                 for (Elf64_Sym& sym : dynsym) {
//                     if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) {
//                         continue;
//                     }

//                     int status;
//                     const char* demangled = abi::__cxa_demangle(&dynstr_data[sym.st_name], NULL, NULL, &status);
//                     std::string sym_name;
//                     if (demangled) {
//                         sym_name = demangled;
//                         free((void*)demangled);
//                     } else {
//                         sym_name = &dynstr_data[sym.st_name];
//                     }
//                     void* sym_addr = (void*)((u8*)base + sym.st_value);
//                     VERBOSE("Dynamic symbol %s at %p (Offset: %zu)", sym_name.c_str(), sym_addr, sym.st_value);
//                     g_symbols[(u64)sym_addr] = sym_name;
//                 }
//                 FELIX86_UNLOCK;
//                 break;
//             }
//         }
//     }

//     Elf64_Shdr strtab{};
//     for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
//         Elf64_Shdr& shdr = shdrtable[i];
//         if (shdr.sh_type == SHT_STRTAB && strcmp(&shstrtab_data[shdr.sh_name], ".strtab") == 0) {
//             strtab = shdr;
//             break;
//         }
//     }

//     if (strtab.sh_type == SHT_STRTAB) {
//         std::vector<char> strtab_data(strtab.sh_size);
//         fseek(file, strtab.sh_offset, SEEK_SET);
//         result = fread(strtab_data.data(), strtab.sh_size, 1, file);
//         if (result != 1) {
//             ERROR("Failed to read string table from file %s", path.c_str());
//         }

//         for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
//             Elf64_Shdr& shdr = shdrtable[i];
//             if (shdr.sh_type == SHT_SYMTAB) {
//                 std::vector<Elf64_Sym> symtab(shdr.sh_size / shdr.sh_entsize);
//                 fseek(file, shdr.sh_offset, SEEK_SET);
//                 result = fread(symtab.data(), shdr.sh_entsize, symtab.size(), file);
//                 if (result != symtab.size()) {
//                     ERROR("Failed to read symbol table from file %s", path.c_str());
//                 }

//                 std::string mangle_buffer;
//                 mangle_buffer.resize(4096);

//                 FELIX86_LOCK;
//                 for (Elf64_Sym& sym : symtab) {
//                     if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) {
//                         continue;
//                     }

//                     int status;
//                     const char* demangled = abi::__cxa_demangle(&strtab_data[sym.st_name], NULL, NULL, &status);
//                     std::string sym_name;
//                     if (demangled) {
//                         sym_name = demangled;
//                         free((void*)demangled);
//                     } else {
//                         sym_name = &strtab_data[sym.st_name];
//                     }
//                     void* sym_addr = (void*)((u8*)base + sym.st_value);
//                     VERBOSE("Symbol %s at %p (Offset: %zu)", sym_name.c_str(), sym_addr, sym.st_value);
//                     g_symbols[(u64)sym_addr] = sym_name;
//                 }
//                 FELIX86_UNLOCK;
//                 break;
//             }
//         }
//     }

//     fclose(file);
// }