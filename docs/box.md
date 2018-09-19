# BOX File Specification

## Introduction
The BOX file container allows to store an arbitrary amount of binary entries filtered by specific filters. 

Each section of the file is not required to be ordered in a specific way, this is managed by using offsets to mark where things are. In this way there's no strict enforcement in how the file should be created as long as all the offsets are correct.

## Central Header

This is the main file header:

Size | Count | Type | Description 
---: | :---: | :---: | ---
4b | 1 | `char[]` | file signature `"box!"`
4b | 1 | `u32` | version (1 for now)
8b | 1 | `u64` | flags
sizeof(SectionHeader) | 1 | SectionHeader | main section header which contains offset to section table
8b | 1 | `u64` | file length
4b | 1 | `u32` | optional crc32 checksum (could be changed in something stronger in the future)

This is the main entry point of the file. The `SectionHeader` tells where a table containing all the section headers for the other sections of the file is stored so that this can be done after reading the whole header.

## Section Header

A section header is meant to store the informations of where a specific section is stored inside the file. Its structure is the following:

Size | Count | Type | Description 
---: | :---: | :---: | ---
8b | 1 | u64 | absolute offset of the section content in the file
8b | 1 | u64 | length in bytes of the section
4b | 1 | u32 | section unique type identifier
4b | 1 | u32 | number of elements of the section




