# BOX File Specification

## Introduction
The BOX file container allows to store an arbitrary amount of binary entries filtered by specific filters. 

Each section of the file is not required to be ordered in a specific way, this is managed by using offsets to mark where things are. In this way there's no strict enforcement in how the file should be created as long as all the offsets are correct.

The general design is that the file contains *n* entries which are mapped to *m* streams (such that multiple entries can be mapped to the same stream as if they were concatenated). Each entry and each stream can have an ordered chain of filters which are applied to the data, each filter can have arbitrary binary data attached.

## Type aliases

These types are used throughout the file format to improve readability:

Type alias | C++ type | Description
:---: | :---: | ---
`offset_t` | `u64` | absolute offset from beginning of file
`count_t` | `u32` | count of elements
`length_t` | `u64` | length in bytes
`index_t` | `s32` | index of element inside array/collection

## Central Header

This is the main file header:

Offset | Type | Description 
---: | :---: | ---
4b | `char[]` | file signature `"box!"`
4b | `u32` | version (1 for now)
8b | `u64` | flags
`sizeof(SectionHeader)` | `SectionHeader` | main section header which contains offset to section table
8b | `length_t` | file length
4b | `u32` | optional crc32 checksum (could be changed in something stronger in the future)

This is the main entry point of the file. The `SectionHeader` tells where a table containing all the section headers for the other sections of the file is stored so that this can be done after reading the whole header.

## Section Header

A section header is meant to store the informations of where a specific section is stored inside the file. Its structure is the following:

Size | Type | Description 
---: | :---: | ---
8b | `offset_t` | absolute offset of the section content in the file
8b | `length_t` | length in bytes of the section
4b | `u32` | section unique type identifier
4b | `count_t` | number of elements of the section

## Entry Header

The entry header stores the information about a binary entry inside the archive. Its structure is the following:

Size | Type | Description 
---: | :---: | ---
8b | `length_t` | filtered size of the entry (necessary? document better)
`sizeof(DigestInfo)` | `DigestInfo` | original length in bytes and additional hash values of the entry
4b | `index_t` | index of stream which contains this entry
4b | `index_t` | index in stream of this entry
8b | `offset_t` | absolute offset in file of payload data
4b | `length_t` | length in bytes of payload data
8b | `timestamp_t ` | timestamp in seconds since 1 January 1970
8b | `offset_t` | absolute offset in file of NUL-terminated entry name

## DigestInfo

This struct stores all the hash related informations of an entry. This could change in the future

Size | Type | Description 
---: | :---: | ---
8b | `length_t` | length in bytes of the entry
8b | `crc32_t` | CRC32 hash of the entry
16b | `byte[16]` | MD5 hash of the entry
20b | `byte[20]` | SHA-1 hash of the entry

## Stream Header

A stream is a single stream of bytes which are altogher filtered with its chain of filters. This means that in the standard situation a box archive will have a stream for each entry but it's possible to concatenate multiple entries and store them as a single stream.





