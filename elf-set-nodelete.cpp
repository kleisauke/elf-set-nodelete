#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Include a local elf.h copy as not all platforms have it.
#include "elf.h"

#define ARRAYELTS(arr) (sizeof (arr) / sizeof (arr)[0])

#define DT_FLAGS_1	0x6ffffffb
#define DF_1_NODELETE	0x00000008	/* Set RTLD_NODELETE for this object. */

bool dry_run = false;
bool quiet = false;

static char const *const usage_message[] =
		{ "\
\n\
Processes ELF files to set RTLD_NODELETE.\n\
\n\
Options:\n\
\n\
--dry-run             print info but do not set the RTLD_NODELETE flag\n\
--quiet               do not print info\n\
--help                display this help and exit\n"
		};

template<typename ElfWord /*Elf{32_Word,64_Xword}*/,
		typename ElfHeaderType /*Elf{32,64}_Ehdr*/,
		typename ElfSectionHeaderType /*Elf{32,64}_Shdr*/,
		typename ElfProgramHeaderType /*Elf{32,64}_Phdr*/,
		typename ElfDynamicSectionEntryType /* Elf{32,64}_Dyn */>
bool process_elf(uint8_t* bytes, size_t elf_file_size, char const* file_name)
{
	if (sizeof(ElfSectionHeaderType) > elf_file_size) {
		fprintf(stderr, "elf-set-nodelete: Elf header for '%s' would end at %zu but file size only %zu\n",
				file_name, sizeof(ElfSectionHeaderType), elf_file_size);
		return false;
	}

	ElfHeaderType* elf_hdr = reinterpret_cast<ElfHeaderType*>(bytes);

	size_t last_section_header_byte = elf_hdr->e_shoff + sizeof(ElfSectionHeaderType) * elf_hdr->e_shnum;
	if (last_section_header_byte > elf_file_size) {
		fprintf(stderr, "elf-set-nodelete: Section header for '%s' would end at %zu but file size only %zu\n",
				file_name, last_section_header_byte, elf_file_size);
		return false;
	}

	ElfSectionHeaderType* section_header_table = reinterpret_cast<ElfSectionHeaderType*>(bytes + elf_hdr->e_shoff);

	/* Iterate over section headers */
	for (unsigned int i = 1; i < elf_hdr->e_shnum; i++) {
		ElfSectionHeaderType* section_header_entry = section_header_table + i;
		if (section_header_entry->sh_type == SHT_DYNAMIC) {
			size_t const last_dynamic_section_byte = section_header_entry->sh_offset + section_header_entry->sh_size;
			if (last_dynamic_section_byte > elf_file_size) {
				fprintf(stderr, "elf-set-nodelete: Dynamic section for '%s' would end at %zu but file size only %zu\n",
						file_name, last_dynamic_section_byte, elf_file_size);
				return false;
			}

			size_t const dynamic_section_entries = section_header_entry->sh_size / sizeof(ElfDynamicSectionEntryType);
			ElfDynamicSectionEntryType* const dynamic_section =
					reinterpret_cast<ElfDynamicSectionEntryType*>(bytes + section_header_entry->sh_offset);

			for (unsigned int j = 0; j < dynamic_section_entries; j++) {
				ElfDynamicSectionEntryType* dynamic_section_entry = dynamic_section + j;

				if (dynamic_section_entry->d_tag == DT_FLAGS_1) {
					// Set RTLD_NODELETE to prevent unload.
					decltype(dynamic_section_entry->d_un.d_val) orig_d_val =
																		dynamic_section_entry->d_un.d_val;
					decltype(dynamic_section_entry->d_un.d_val) new_d_val =
																		(orig_d_val | DF_1_NODELETE);
					if (new_d_val != orig_d_val) {
						if (!quiet)
							printf("elf-set-nodelete: Replacing DF_1_* flags %llu with %llu in '%s'\n",
								   (unsigned long long) orig_d_val,
								   (unsigned long long) new_d_val,
								   file_name);
						if (!dry_run)
							dynamic_section_entry->d_un.d_val = new_d_val;
					}
				}
			}
		}
	}

	return true;
}


int main(int argc, char const** argv)
{
	if (argc == 1 || (argc == 2 && strcmp(argv[1], "--help") == 0)) {
		printf("Usage: %s [OPTIONS] [FILENAME]...\n", argv[0]);
		for (unsigned int i = 0; i < ARRAYELTS(usage_message); i++)
			fputs(usage_message[i], stdout);
		exit(0);
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--dry-run") == 0) {
			dry_run = true;
			continue;
		}

		if (strcmp(argv[i], "--quiet") == 0) {
			quiet = true;
			continue;
		}

		char const* file_name = argv[i];
		int fd = open(file_name, O_RDWR);
		if (fd < 0) {
			char* error_message;
			if (asprintf(&error_message, "open(\"%s\")", file_name) == -1)
				error_message = (char*) "open()";
			perror(error_message);
			return 1;
		}

		struct stat st;
		if (fstat(fd, &st) < 0) { perror("fstat()"); return 1; }

		if (st.st_size < (long long) sizeof(Elf32_Ehdr)) {
			close(fd);
			continue;
		}

		void* mem = mmap(0, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED) { perror("mmap()"); return 1; }

		uint8_t* bytes = reinterpret_cast<uint8_t*>(mem);
		if (!(bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F')) {
			// Not the ELF magic number.
			munmap(mem, st.st_size);
			close(fd);
			continue;
		}

		if (bytes[/*EI_DATA*/5] != 1) {
			fprintf(stderr, "elf-set-nodelete: Not little endianness in '%s'\n",
					file_name);
			munmap(mem, st.st_size);
			close(fd);
			continue;
		}

		uint8_t const bit_value = bytes[/*EI_CLASS*/4];
		if (bit_value == 1) {
			if (!process_elf<Elf32_Word, Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr, Elf32_Dyn>(bytes, st.st_size, file_name))
				return 1;
		} else if (bit_value == 2) {
			if (!process_elf<Elf64_Xword, Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr, Elf64_Dyn>(bytes, st.st_size, file_name))
				return 1;
		} else {
			fprintf(stderr, "elf-set-nodelete: Incorrect bit value %d in '%s'\n",
					bit_value, file_name);
			return 1;
		}

		if (msync(mem, st.st_size, MS_SYNC) < 0) {
			perror("msync()");
			return 1;
		}

		munmap(mem, st.st_size);
		close(fd);
	}
	return 0;
}
