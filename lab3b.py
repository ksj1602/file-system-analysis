#!/usr/bin/env python3

# NAME: Kshitij Jhunjhunwala
# EMAIL: ksj1602@ucla.edu

from collections import defaultdict
import sys

if len(sys.argv) != 2:
    sys.stderr.write('Usage ./lab3b [.csv FILE]\n')
    exit(1)

csv_file = sys.argv[1]

if csv_file[-4:] != '.csv':
    sys.stderr.write("Input file must be a '.csv' file\n")
    exit(1)

try:
    csv_file = open(sys.argv[1], 'r')
except OSError as e:
    sys.stderr.write('Cannot open "' + sys.argv[1] + '": ' + e.strerror + '\n')
    exit(1)

# set for error messages to prevent duplicate error messages
error_msgs = set()

orig_free_inodes = set()  # set to store inodes marked free in the file
orig_free_blocks = set()  # same as above but for blocks
my_block_bit_map = defaultdict(list)  # dictionary to store block data
unallocated_inodes = {2}  # set to track unallocated inodes
unreferenced_blocks = set()  # set to track unallocated blocks
orig_inode_link_counts = {}  # dictionary for inode to linkcount mapping
my_inode_link_counts = {}  # same as above but stores what is actually found
super_block_entry = None  # variable to store superblock data
group_entry = None  # variable to store group descriptor
inode_entries = []  # list for inode entries
directory_entries = []  # list for directory entries
indirect_entries = []  # list of indirect block data entries

# miscellaneous variables to keep track of various quantities

number_of_blocks = 0  # stores total number of blocks
number_of_inodes = 0  # stores total number of inodes
itable_blocks = 0  # stores number of blocks in the inode table
first_non_reserved_block = 8  # self-explanatory
reserved_blocks = {0, 1, 2}  # set for reserved block numbers
reserved_inodes = {1}  # set for reserved inode numbers
parent_inode_map = {}  # dictionary to map child inodes to parent inodes
inconsistency_found = False  # variable to track if error has been found

# the parent of the root directory is itself
parent_inode_map[2] = 2

file_lines = csv_file.readlines()

# Get all input into variables first
for line in file_lines:
    line = line.rstrip()  # removes trailing newline
    current_entry = line.split(',')

    entry_type = current_entry[0]

    if entry_type == 'SUPERBLOCK':
        super_block_entry = current_entry
        number_of_blocks = int(current_entry[1])
        reserved_blocks.add(number_of_blocks)
        number_of_inodes = int(current_entry[2])
        inodes_per_block = int(current_entry[3]) // int(current_entry[4])
        itable_blocks = int(current_entry[6]) // inodes_per_block
        first_non_reserved_inode = int(current_entry[7])

        # all inodes from 1 to first non reserved inode are reserved except
        # for inode 2, which is for the root directory
        for v in range(3, first_non_reserved_inode):
            reserved_inodes.add(v)

        # initially all inodes are unallocated
        # they will be removed from the set as they are processed
        for v in range(first_non_reserved_inode, number_of_inodes + 1):
            unallocated_inodes.add(v)

    # the first non-reserved block is the one after the inode table
    elif entry_type == 'GROUP':
        first_non_reserved_block = int(current_entry[7]) + itable_blocks + 1
        for v in range(0, first_non_reserved_block):
            reserved_blocks.add(v)
        for v in range(first_non_reserved_block, number_of_blocks):
            unreferenced_blocks.add(v)

    elif entry_type == 'BFREE':
        orig_free_blocks.add(int(current_entry[1]))

    elif entry_type == 'IFREE':
        orig_free_inodes.add(int(current_entry[1]))

    elif entry_type == 'INODE':
        inode_entries.append(current_entry)

    elif entry_type == 'DIRENT':
        directory_entries.append(current_entry)
        if current_entry[6] != "'.'" and current_entry[6] != "'..'":
            parent_inode = int(current_entry[1])
            child_inode = int(current_entry[3])
            parent_inode_map[child_inode] = parent_inode

    elif entry_type == 'INDIRECT':
        indirect_entries.append(current_entry)


# function to generate error messages for blocks
def get_block_errors(block_num, inode_number, off_set, level):
    global inconsistency_found

    if block_num != 0:
        if block_num < 0 or block_num > number_of_blocks:
            error_msgs.add('INVALID ' + level +
                           'BLOCK ' + str(block_num) +
                           ' IN INODE ' + str(inode_number) +
                           ' AT OFFSET ' + str(off_set))
            inconsistency_found = True

        if block_num in reserved_blocks:
            error_msgs.add('RESERVED ' + level +
                           'BLOCK ' + str(block_num) +
                           ' IN INODE ' + str(inode_number) +
                           ' AT OFFSET ' + str(off_set))
            inconsistency_found = True

        if block_num in orig_free_blocks:
            error_msgs.add(
                'ALLOCATED BLOCK ' + str(block_num) +
                ' ON FREELIST')
            inconsistency_found = True

        if block_num in my_block_bit_map:
            my_block_bit_map[block_num].append([level, block_num, inode_number,
                                                off_set])
            for blk in my_block_bit_map[block_num]:
                error_msgs.add('DUPLICATE ' + blk[0] +
                               'BLOCK ' + str(blk[1]) +
                               ' IN INODE ' + str(blk[2]) +
                               ' AT OFFSET ' + str(blk[3]))
            inconsistency_found = True

        my_block_bit_map[block_num].append([level, block_num, inode_number,
                                            off_set])


# process every inode entry
for current_entry in inode_entries:
    file_type = current_entry[2]
    file_size = int(current_entry[10])
    current_inode_number = int(current_entry[1])
    orig_inode_link_counts[current_inode_number] = int(current_entry[6])

    # since this inode is allocated, remove it from unallocated inodes
    unallocated_inodes.remove(current_inode_number)

    # get the number of block pointers in every block
    block_size = int(super_block_entry[3])
    indirect_pointers = block_size // 4

    # check if this inode is not on the freelist
    if current_inode_number in orig_free_inodes:
        error_msgs.add('ALLOCATED INODE ' + str(current_inode_number) +
                       ' ON FREELIST')
        inconsistency_found = True

    # if we expect block numbers to be output, then we process them
    if file_type == 'f' or file_type == 'd' or file_size > 60:
        for j in range(12, 27):
            current_block_number = int(current_entry[j])
            if j == 24:
                indirection_level = 'INDIRECT '
                offset = 12
            elif j == 25:
                indirection_level = 'DOUBLE INDIRECT '
                offset = 12 + indirect_pointers
            elif j == 26:
                indirection_level = 'TRIPLE INDIRECT '
                offset = 12 + indirect_pointers + int(pow(indirect_pointers, 2))
            else:
                indirection_level = ''
                offset = j - 12

            get_block_errors(current_block_number, current_inode_number,
                             offset, indirection_level)

# after all inodes have been processed, if any unallocated inodes
# are not on the freelist, output errors
for inode in unallocated_inodes:
    if inode not in orig_free_inodes:
        error_msgs.add('UNALLOCATED INODE ' + str(inode) + ' NOT ON FREELIST')
        inconsistency_found = True

# check for block errors in indirect entries
for entry in indirect_entries:
    current_inode_number = int(entry[1])
    indirection_level = int(entry[2])
    if indirection_level == 1:
        indirection_level = 'INDIRECT '
    elif indirection_level == 2:
        indirection_level = 'DOUBLE INDIRECT '
    else:
        indirection_level = 'TRIPLE INDIRECT '
    offset = int(entry[3])
    current_block_number = int(entry[5])

    get_block_errors(current_block_number, current_inode_number,
                     offset, indirection_level)

# after all block references have been processed (inodes + indirect entries)
# we will look to see if any unreferenced blocks are not on the freelist
for block in unreferenced_blocks:
    if block not in orig_free_blocks and block not in my_block_bit_map:
        error_msgs.add('UNREFERENCED BLOCK ' + str(block))
        inconsistency_found = True

# Now we process directory entries
for dirent in directory_entries:
    parent_inode_number = int(dirent[1])
    ref_inode_number = int(dirent[3])
    entry_name = dirent[6]

    if ref_inode_number < 1 or ref_inode_number > number_of_inodes:
        error_msgs.add('DIRECTORY INODE ' + str(parent_inode_number) +
                       ' NAME ' + entry_name +
                       ' INVALID INODE ' + str(ref_inode_number))
        inconsistency_found = True

    if ref_inode_number in unallocated_inodes:
        error_msgs.add('DIRECTORY INODE ' + str(parent_inode_number) +
                       ' NAME ' + entry_name +
                       ' UNALLOCATED INODE ' + str(ref_inode_number))
        inconsistency_found = True

    if entry_name == "'.'" and parent_inode_number != ref_inode_number:
        error_msgs.add('DIRECTORY INODE ' + str(parent_inode_number) +
                       ' NAME ' + entry_name +
                       ' LINK TO INODE ' + str(ref_inode_number) +
                       ' SHOULD BE ' + str(parent_inode_number))
        inconsistency_found = True

    correct_parent = parent_inode_map[parent_inode_number]
    if entry_name == "'..'" and ref_inode_number != correct_parent:
        error_msgs.add('DIRECTORY INODE ' + str(parent_inode_number) +
                       ' NAME ' + entry_name +
                       ' LINK TO INODE ' + str(ref_inode_number) +
                       ' SHOULD BE ' + str(correct_parent))
        inconsistency_found = True

    my_inode_link_counts[ref_inode_number] = \
        my_inode_link_counts.get(ref_inode_number, 0) + 1

# We make one final pass through all the inode entries
# to check for link count errors
for inode in inode_entries:
    num = int(inode[1])
    link_count = int(inode[6])
    actual_links = my_inode_link_counts.get(num, 0)

    if actual_links != link_count:
        error_msgs.add('INODE ' + str(num) + ' HAS ' + str(actual_links) +
                       ' LINKS BUT LINKCOUNT IS ' + str(link_count))
        inconsistency_found = True

# print out all the error messages
for msg in error_msgs:
    print(msg)

# exit with the correct code
if inconsistency_found:
    exit(2)
else:
    exit(0)
