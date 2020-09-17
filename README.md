# File System Analysis

This is a C program to extract information about files from a raw disk image using the EXT2 file system.

It prints data about the superblock, group descriptor, free blocks, free inodes, information about used inodes, directory entries, etc. in a comma-separated manner. The data is printed as it exists on the image.

To build and run the program, run the following from your terminal:

```
$ git clone https://github.com/ksj1602/file-system-analysis.git
$ make
$ ./extract-info [IMAGE FILE NAME]
```
This will print the information to standard output.

It is possible that the information is inconsistent, since the program simply extracts information and prints it.

To check for errors, there is a python script `check.py`. Execute it using:

```
$ ./check.py
```
If you get a `Permission denied` error, use the following command to grant it execution permissions:

```
$ chmod +x check.py
```
Note: This project was prepared as part of UCLA coursework (CS 111: Operating Systems Principles) taken Summer 2020. Please do not copy it for any future offerings. I am not responsible if you are caught for cheating.