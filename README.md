# Raison d'etre
This project is aimed to providing a flexible archive format which is mainly aimed at storing dumps of preserved media (games as ROMs or disk images) in an efficient way.

This is done through multiple choices:
* multiple hash (crc32, md5 and sha1) data is stored directly in the header of the file to avoid any computation
* it supports arbitrary stream filtering (LZMA, deflate, xdelta3) which allows to store and entry as compressed or as a delta from another file

In addition this project is trying to provide a fuse implementation meant to keep hashed data organized.
