/*
    NAME: Kshitij Jhunjhunwala
    EMAIL: ksj1602@ucla.edu
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

// EXT2 header file
#include "ext2_fs.h"

__u32 block_size = EXT2_MIN_BLOCK_SIZE;
int disk_image = 0;

unsigned int get_block_offset(unsigned block_num)
{
    return block_size * block_num;
}

// Wrapper for pread
long Pread(int fd, void* buf, size_t nbytes, off_t offset)
{
    ssize_t bytes_to_be_read = nbytes;
    ssize_t bytes_read = 0;
    ssize_t temp;
    while((temp = pread(fd, buf + bytes_read, nbytes - bytes_read, offset + bytes_read)) < bytes_to_be_read)
    {
        if (temp == -1)
        {
            fprintf(stderr, "Internal program error: %s\n", strerror(errno));
            exit(2);
        }
        bytes_read += temp;
    }
    return bytes_to_be_read;
}

void print_directory_entry_block (__u32 block_num, __u32 parent_inode)
{
    __u32 entry_offset = block_num * block_size;
    __u32 logical_offset = 0;
    struct ext2_dir_entry current_entry;
    char name_buffer[256];
    while (logical_offset < block_size)
    {
        Pread(disk_image, &current_entry, sizeof(current_entry), entry_offset);
        if (current_entry.inode != 0)
        {
            printf("DIRENT,%u,%u,%u,%u,%u,", parent_inode, logical_offset, current_entry.inode, current_entry.rec_len,
                    current_entry.name_len);
            memset(name_buffer, 0, 256 * sizeof(char));
            for(__u32 k = 0; k < current_entry.name_len; k++)
            {
                name_buffer[k] = current_entry.name[k];
            }
            printf("'%s'\n", name_buffer);
        }
        logical_offset += current_entry.rec_len;
        entry_offset += current_entry.rec_len;
    }
}

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: ./lab3a [IMAGE FILE]\n");
        exit(1);
    }
    disk_image = open(argv[1], O_RDONLY);
    if (disk_image == -1)
    {
        fprintf(stderr, "Cannot open file '%s': %s\n", argv[1], strerror(errno));
        exit(1);      
    }


    // print out superblock description
    struct ext2_super_block super_block;

    Pread(disk_image, &super_block, sizeof(super_block), 1024);
    block_size = 1024 << super_block.s_log_block_size;
    printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", super_block.s_blocks_count,
            super_block.s_inodes_count, block_size, super_block.s_inode_size,
            super_block.s_blocks_per_group, super_block.s_inodes_per_group, super_block.s_first_ino);

    u_int32_t block_count = super_block.s_blocks_count;
    u_int32_t inode_count = super_block.s_inodes_count;
    u_int32_t group_offset = (block_size > 1024) ? 1 : 2;

    // print out group description
    struct ext2_group_desc grp_dsc;
    Pread(disk_image, &grp_dsc, sizeof(grp_dsc), block_size * group_offset);

    printf("GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n", 0, super_block.s_blocks_count,
            super_block.s_inodes_count, grp_dsc.bg_free_blocks_count,
            grp_dsc.bg_free_inodes_count, grp_dsc.bg_block_bitmap,
            grp_dsc.bg_inode_bitmap, grp_dsc.bg_inode_table);
    
    
    // printing out free blocks    
    u_int32_t block_bitmap_offset = grp_dsc.bg_block_bitmap * block_size;
    u_int8_t block_bitmap_buffer[block_size];

    Pread(disk_image, block_bitmap_buffer, block_size, block_bitmap_offset);
    for (u_int32_t block_number = 1; block_number <= block_count; block_number++)
    {
        int block_byte = (block_number - 1) / 8;
        int block_bit_offset = (block_number - 1) % 8;
        if (!(block_bitmap_buffer[block_byte] & (1 << block_bit_offset)))
        {
            printf("BFREE,%u\n", block_number);
        }
    }

    // print out free inodes
    u_int32_t inode_bitmap_offset = grp_dsc.bg_inode_bitmap * block_size;
    u_int8_t inode_bitmap_buffer[block_size];

    Pread(disk_image, inode_bitmap_buffer, block_size, inode_bitmap_offset);
    for (u_int32_t inode_number = 1; inode_number <= inode_count; inode_number++)
    {
        int inode_byte = (inode_number - 1) / 8;
        int inode_bit_offset = (inode_number - 1) % 8;
        if (!(inode_bitmap_buffer[inode_byte] & (1 << inode_bit_offset)))
        {
            printf("IFREE,%u\n", inode_number);
        }
    }


    // process each inode
    u_int32_t inode_table_offset = grp_dsc.bg_inode_table * block_size;
    struct ext2_inode inode_table_buffer[inode_count];

    Pread(disk_image, inode_table_buffer, inode_count * sizeof(struct ext2_inode), inode_table_offset);
    for (__u32 i = 0; i < inode_count; i++)
    {
        if (inode_table_buffer[i].i_links_count > 0 && inode_table_buffer[i].i_mode)
        {
            // print out inode entry

            printf("INODE,%u,", (i + 1));
            __u16 mode = inode_table_buffer[i].i_mode;
            char file_type = S_ISDIR(mode) ? 'd' : (S_ISREG(mode) ? 'f' : (S_ISLNK(mode) ? 's' : '?'));
            printf("%c,%o,%u,%u,%u", file_type, (mode & 0xfff),
                    inode_table_buffer[i].i_uid, inode_table_buffer[i].i_gid,
                    inode_table_buffer[i].i_links_count);
            
            time_t te = (time_t) inode_table_buffer[i].i_ctime;
            struct tm *gm_time = gmtime(&te);
            printf(",%.2d/%.2d/%.2d %.2d:%.2d:%.2d", gm_time->tm_mon + 1, gm_time->tm_mday,
                    gm_time->tm_year % 100, gm_time->tm_hour, gm_time->tm_min, gm_time->tm_sec);

            te = (time_t) inode_table_buffer[i].i_mtime;
            gm_time = gmtime(&te);
            printf(",%.2d/%.2d/%.2d %.2d:%.2d:%.2d", gm_time->tm_mon + 1, gm_time->tm_mday,
                    gm_time->tm_year % 100, gm_time->tm_hour, gm_time->tm_min, gm_time->tm_sec);

            te = (time_t) inode_table_buffer[i].i_atime;
            gm_time = gmtime(&te);
            printf(",%.2d/%.2d/%.2d %.2d:%.2d:%.2d", gm_time->tm_mon + 1, gm_time->tm_mday,
                    gm_time->tm_year % 100, gm_time->tm_hour, gm_time->tm_min, gm_time->tm_sec);

            printf(",%u,%u", inode_table_buffer[i].i_size, inode_table_buffer[i].i_blocks);


            // print out inode_table[0-15] if required
            if (file_type == 'f' || file_type == 'd' || (file_type == 's' && inode_table_buffer[i].i_size > 60))
            {
                for (__u16 j = 0; j < EXT2_N_BLOCKS; j++)
                {
                    printf(",%u", inode_table_buffer[i].i_block[j]);
                }
                printf("\n");
            }
            else
            {
                printf("\n");
            }

            
            // if inode is a directory, print entries found in direct blocks
            for (__u16 j = 0; j < 12; j++)
            {
                if (inode_table_buffer[i].i_block[j] && file_type == 'd')
                {
                    print_directory_entry_block(inode_table_buffer[i].i_block[j], (i + 1));
                }
            }

            // singly indirect blocks
            if (inode_table_buffer[i].i_block[12])
            {
                // store block number of indirect block in variable
                __u32 sind_block_num = inode_table_buffer[i].i_block[12];

                // array to hold block numbers of data blocks
                __u32 data_block_nums[(block_size / 4)];

                // read block numbers into above array
                Pread(disk_image, data_block_nums, block_size, sind_block_num * block_size);

                for (__u32 v = 0; v < (block_size / 4); v++)
                {
                    // if there is a non-zero block number, we scan it for data
                    if (data_block_nums[v])
                    {
                        // indirection message
                        printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 1, 12 + v, sind_block_num, data_block_nums[v]);

                        // if the current file is a directory, print out the entries
                        if (file_type == 'd')
                        {
                            print_directory_entry_block(data_block_nums[v], (i + 1));
                        }
                    }
                }
                
            }

            //doubly indirect blocks : simply repeat process above twice
            if (inode_table_buffer[i].i_block[13])
            {
                __u32 dind_block_num = inode_table_buffer[i].i_block[13]; // block number of double indirect block
                __u32 single_indirect_block_nums[(block_size / 4)]; // array for single indirect block numbers

                Pread(disk_image, single_indirect_block_nums, block_size, dind_block_num * block_size);

                for (__u32 v = 0; v < (block_size / 4); v++)
                {
                    if (single_indirect_block_nums[v])
                    {
                        printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 2, 12 + (block_size / 4) + v * (block_size / 4), 
                                dind_block_num, single_indirect_block_nums[v]);                        

                        __u32 data_block_nums[(block_size / 4)];
                        Pread(disk_image, data_block_nums, block_size, block_size * single_indirect_block_nums[v]);

                        for (__u32 w = 0; w < (block_size / 4); w++)
                        {
                            if (data_block_nums[w])
                            {
                                printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 1, 12 + (block_size / 4) + v * (block_size / 4) + w, 
                                        single_indirect_block_nums[v], data_block_nums[w]);
                                
                                if (file_type == 'd')
                                {
                                    print_directory_entry_block(data_block_nums[w], (i + 1));
                                }
                                
                            }
                        }
                        
                    }
                }
            }

            //triply indirect blocks: repeat procedure for singly indirect blocks thrice
            if (inode_table_buffer[i].i_block[14])
            {
                __u32 tind_block_num = inode_table_buffer[i].i_block[14]; // block number of triple indirect block
                __u32 double_indirect_block_nums[(block_size / 4)]; // array for double indirect block numbers

                // storing value of logical offset in variable to avoid recomputation and long lines of code
                __u32 base_logical_offset = 12 + (block_size / 4) + ((block_size * block_size) / 16);
                Pread(disk_image, double_indirect_block_nums, block_size, tind_block_num * block_size);

                for (__u32 v = 0; v < (block_size / 4); v++)
                {
                    if (double_indirect_block_nums[v])
                    {
                        printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 3, base_logical_offset + v * ((block_size * block_size) / 16),
                                tind_block_num, double_indirect_block_nums[v]);

                        __u32 single_indirect_block_nums[(block_size / 4)];
                        Pread(disk_image, single_indirect_block_nums, block_size, double_indirect_block_nums[v] * block_size);
                        
                        // storing value of logical offset in variable to avoid recomputation and long lines of code
                        __u32 base_logical_offset2 = base_logical_offset + v * ((block_size * block_size) / 16);
                        
                        for (__u32 w = 0; w < (block_size / 4); w++)
                        {
                            if (single_indirect_block_nums[w])
                            {
                                printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 2, base_logical_offset2 + w * (block_size / 4), 
                                        double_indirect_block_nums[v], single_indirect_block_nums[w]);	

                                __u32 data_block_nums[(block_size / 4)];
                                Pread(disk_image, data_block_nums, block_size, single_indirect_block_nums[w] * block_size);

                                // storing value of logical offset in variable to avoid recomputation and long lines of code
                                __u32 base_logical_offset3 = base_logical_offset2 + w * (block_size / 4);

                                for (__u32 y = 0; y < (block_size / 4); y++)
                                {
                                    if (data_block_nums[y])
                                    {
                                        printf("INDIRECT,%u,%u,%u,%u,%u\n", (i + 1), 1, base_logical_offset3 + y,
                                            single_indirect_block_nums[w], data_block_nums[y]);
                                        
                                        if (file_type == 'd')
                                        {
                                            print_directory_entry_block(data_block_nums[y], (i + 1));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}