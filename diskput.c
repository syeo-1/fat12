#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "common_functions.h"

#define SEC_BYTE_SIZE 512



char * capitalize(char* dirname) {
    char* caps_dirname = malloc(sizeof(char) * (strlen(dirname)+1));
    for(int i = 0 ; i < strlen(dirname) ; i++) {
        caps_dirname[i] = toupper(dirname[i]);
    }
    caps_dirname[strlen(dirname)] = '\0';
    return caps_dirname;
}

void set_file_size(char* p, int filesize, int dir_entry_loc) {
    unsigned char lowest = filesize & 0x000000ff;
    unsigned char midlow = (filesize & 0x0000ff00) >> 8;
    unsigned char midhigh = (filesize & 0x00ff0000) >> 20;
    unsigned char high = (filesize & 0xff000000) >> 24;

    p[dir_entry_loc+28] = lowest;
    p[dir_entry_loc+29] = midlow;
    p[dir_entry_loc+30] = midhigh;
    p[dir_entry_loc+31] = high;
}

void set_first_logical_cluster(char* p, int first_logic_cluster, int dir_entry_loc) {
    unsigned short short_logic_cluster_value = first_logic_cluster;
    p[dir_entry_loc+26] = short_logic_cluster_value;
}

void set_fat_entry(char* p, int prev_fat_entry, int cur_fat_entry) {
    // set the fat entry value of the prev fat entry to be the current in the FAT table
    int fat1_start = 512;
    // int fat_value;

    if (prev_fat_entry == cur_fat_entry) {// set the last sector value 0xfff
        int four_bits = 0x0f;
        int byte = 0xff;

        if (prev_fat_entry%2 == 0) {
            // get 4 bits location
            int four_bit_loc = fat1_start+(1+(3*prev_fat_entry)/2);
            // get 8 bits location
            int byte_loc = fat1_start+(3*prev_fat_entry)/2;
            p[four_bit_loc] += four_bits;// could be overwriting somethings, so +=, not =
            p[byte_loc] = byte;
        } else {
            // get high 4 bits in this location
            int four_bit_loc = fat1_start+(3*prev_fat_entry)/2;
            // get 8 bits in this location, ie. 1 byte
            int byte_loc = fat1_start+(1+(3*prev_fat_entry)/2);

            four_bits = four_bits << 4;
            p[four_bit_loc] += four_bits;// could be overwriting something
            p[byte_loc] = byte;
        }
    } else if (prev_fat_entry%2 == 0) { // even
        // get 4 bits location
        int four_bit_loc = fat1_start+(1+(3*prev_fat_entry)/2);
        // get 8 bits location
        int byte_loc = fat1_start+(3*prev_fat_entry)/2;

        // get the values
        int four_bits = cur_fat_entry & 0x0f00;
        int byte = cur_fat_entry & 0x00ff;

        four_bits = four_bits >> 8;

        // set previous fat entry to point to current fat entry
        p[four_bit_loc] += four_bits;
        p[byte_loc] = byte;


    } else { // odd
        // get high 4 bits in this location
        int four_bit_loc = fat1_start+(3*prev_fat_entry)/2;
        // get 8 bits in this location, ie. 1 byte
        int byte_loc = fat1_start+(1+(3*prev_fat_entry)/2);

        // get the values
        int four_bits = cur_fat_entry & 0x000f;
        int byte = cur_fat_entry & 0x0ff0;

        byte = byte >> 4;
        four_bits = four_bits << 4;

        // set previous fat entry to point to current fat entry
        p[four_bit_loc] += four_bits;
        p[byte_loc] = byte;

    }
}

int find_free_fat_entry(char* p, int prev_used) {

    int max_fat_entries = 2848;

    int entry_number = 2;// skip entry 0 and entry 1

    if (prev_used == -1) { // first fat_entry value for copying
        while (entry_number <= max_fat_entries ) {
            int fat_value = get_fat_entry_value(p, entry_number);
            if (fat_value == 0) {
                return entry_number;
            }
            entry_number++;
        }
    } else {
       while (entry_number <= max_fat_entries) {
            int fat_value = get_fat_entry_value(p, entry_number);
            if (fat_value == 0 && entry_number > prev_used) { // next fat entry will be further in table than previous
                return entry_number;
            }
            entry_number++;
        } 
    }
    
    // no free fat entries
    return -1;
}

void create_new_dir_entry(char* p, int dir_location, char* filename, int filesize) {

    // find first free directory entry in the current directory
    int first_free_entry_loc;
    for (int i = dir_location ; i < dir_location+512 ; i+=32) {
        if (p[i] == 0x00 || p[i] == 0xe5) {
            first_free_entry_loc = i;
            break;
        }
    }

    char* caps_filename = capitalize(filename);

    char* name;
    char* ext;

    // separate filename into the name and extension
    const char dot[2] = ".";
    char *token;
    token = strtok(caps_filename, dot);

    int token_num = 1;
    while(token != NULL) {
        if (token_num == 1) {
            name = token;
        } else {
            ext = token;
            break;
        }
        token_num++;
        token = strtok(NULL, dot);
    }

    // write in name
    for (int i = first_free_entry_loc, j = 0; i < first_free_entry_loc+8 ; i++, j++) {
        if (j >= strlen(name)) {
            p[i] = ' ';
        } else {
            p[i] = name[j];
        }
    }

    // write in extension
    for (int i = first_free_entry_loc+8, j = 0 ; i < first_free_entry_loc+11 ; i++, j++) {
        if (j >= strlen(ext)) {
            p[i] = ' ';
        } else {
            p[i] = ext[j];
        }
    }

    // open the appropriate file in current linux directory for reading
    FILE *fp;
    fp = fopen(filename, "r");

    int first_free_fat_entry_num = find_free_fat_entry(p, -1);
    int first_logical_cluster = first_free_fat_entry_num;

    int prev_free_fat_entry_num = first_free_fat_entry_num;

    int current_free_fat_entry_num;
    int current_byte = 0;
    // go through fat entry values to find unused sectors in memory 
    while (current_byte < filesize) {

        // calculate corresponding physical sector for FAT entry
        int physical_sector_byte = (33 + prev_free_fat_entry_num - 2) * SEC_BYTE_SIZE;

        // copy to appropriate cluster in data area
        int cluster_bytes_copied = 0;
        while (cluster_bytes_copied < SEC_BYTE_SIZE && current_byte < filesize) {
            char single_byte = fgetc(fp);
            p[physical_sector_byte + cluster_bytes_copied] = single_byte;
            cluster_bytes_copied++;
            current_byte++;
        }

        // file size not yet reached
        if (current_byte < filesize) {
            current_free_fat_entry_num = find_free_fat_entry(p, prev_free_fat_entry_num);
            // set the prev fat entry value in table to be value of current free fat entry num
            // "previous" fat entry now points to current
            set_fat_entry(p, prev_free_fat_entry_num, current_free_fat_entry_num);

            prev_free_fat_entry_num = current_free_fat_entry_num;
        }
    }

    if(filesize <= 512) {
        current_free_fat_entry_num = prev_free_fat_entry_num;
    }
    set_fat_entry(p, prev_free_fat_entry_num, current_free_fat_entry_num);

    set_first_logical_cluster(p, first_logical_cluster, first_free_entry_loc);
    set_file_size(p, filesize, first_free_entry_loc);

    // close the file after finished reading
    fclose(fp);
}

int file_in_current_dir(char* filename) {
    struct stat st;
    if (stat(filename, &st)) {
        return 0;
    }
    return 1;
}

int filesize_current_dir(char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

int dirpath_exists(char* p, char* user_dirpath, char* check_dirpath, int start, int end) {
    if (strcmp(user_dirpath, "/") == 0 || strcmp(user_dirpath, "") == 0) {// dirpath is just root directory
        return 19 * SEC_BYTE_SIZE;// default will be the root directory
    } else { // recurse and build up all possible dirpaths and compare with given dirpath
        // start off with going through root directory
        if (start == 9728) {
            // recurse on any subdirectories in root directory
            for(int i = start ; i < end ; i+=32) { 
                char data_type = get_dir_or_file(p, i);
                if (data_type == 'D') {
                    unsigned short first_logic_cluster = p[i+26];
                    int physical_cluster_num = first_logic_cluster+31;
                    char cur_dirname[8];
                    get_dirname(p, cur_dirname, i);
                    char combined_dirname[strlen(cur_dirname)+2];
                    combined_dirname[0] = '\0';
                    strncat(combined_dirname, "/", 1);
                    strncat(combined_dirname, cur_dirname, strlen(cur_dirname));

                    // match found
                    if (strcmp(user_dirpath, check_dirpath) == 0) {
                        return start;
                    }
                    int subdir_start = physical_cluster_num*512;
                    int subdir_end = subdir_start+512;
                    return dirpath_exists(p, user_dirpath, combined_dirname, subdir_start, subdir_end);
                }
            }
        } else {
            // recurse on subdirectories in current subdirectory
            for (int i = start ; i < end ; i+=32) {
                char data_type = get_dir_or_file(p, i);
                if (data_type == 'D') {
                    unsigned short first_logic_cluster = p[i+26];
                    int physical_cluster_num = first_logic_cluster+31;
                    char cur_dir_name[8];
                    get_dirname(p, cur_dir_name, i);

                    // don't recurse on current or parent directory
                    if (strcmp(cur_dir_name, ".") == 0 || strcmp(cur_dir_name, "..") == 0) {
                        continue;
                    }

                    char combined_dirname[strlen(check_dirpath)+strlen(cur_dir_name)+2];
                    combined_dirname[0] = '\0';
                    strncat(combined_dirname, check_dirpath, strlen(check_dirpath));
                    strncat(combined_dirname, "/", 1);
                    strncat(combined_dirname, cur_dir_name, strlen(cur_dir_name));

                    // match found
                    if (strcmp(user_dirpath, check_dirpath) == 0) {
                        return start;
                    }
                    int subdir_start = physical_cluster_num*512;
                    int subdir_end = subdir_start+512;
                    return dirpath_exists(p, user_dirpath, combined_dirname, subdir_start, subdir_end);
                } 
            }
        }
    }

    // final check
    if (strcmp(user_dirpath, check_dirpath) == 0) {
        return start;
    }

    // path does not exist
    return -1;
}

void file_from_path(char* path, char* full_filename, int sep_index) {
    char filename[12];
    int filename_index = 0;

    if (sep_index == 0) {
        for (int i = sep_index ; i < strlen(path) ; i++) {
            filename[filename_index] = path[i];
            filename_index++;
        } 
    } else {
        for (int i = sep_index+1 ; i < strlen(path) ; i++) {
            filename[filename_index] = path[i];
            filename_index++;
        }
    }

    filename[filename_index] = '\0';
    full_filename[filename_index] = '\0';

    strncpy(full_filename, filename, filename_index);
}

int get_slash_seperator_index(char* path) {
    int seperator_index;
    for (int i = strlen(path) ; i >= 0 ; i--) {
        if (path[i] == '/') {
            seperator_index = i;
            break;
        }
    }
    return seperator_index;
}

int main(int argc, char *argv[]) {
    int fd;
	struct stat sb;

    if (argc == 3) {
        fd = open(argv[1], O_RDWR);
        fstat(fd, &sb);

        char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            printf("Error: failed to map memory\n");
            exit(1);
        }

        // parse the path argument to get the filename

        // get the separator index
        int sep_index = get_slash_seperator_index(argv[2]);

        char full_filename[12];
        file_from_path(argv[2], full_filename, sep_index);

        if (file_in_current_dir(full_filename) == 0) {
            printf("not in directory\n");
            exit(1);
        }

        // get the directory path
        char full_dirpath[100]; // should be long enough
        strncpy(full_dirpath, argv[2], sep_index);
        char* caps_dirname = capitalize(full_dirpath);
        full_filename[12] = '\0';

        // check if the path exists in the fat12 image
        int dir_location = dirpath_exists(p, caps_dirname, "/", 19*SEC_BYTE_SIZE, 33*SEC_BYTE_SIZE); 
        if (dir_location == -1) {
            printf("path does not exist\n");
            exit(1);
        } 

        // get free space on the disk
        int free_diskspace = get_free_size_on_disk(p);

        // get size of the file
        int filesize = filesize_current_dir(full_filename);
        if (free_diskspace < filesize) {
            printf("not enough free space in disk image\n");
            exit(1);
        }

        // create a new directory entry in the given directory
        create_new_dir_entry(p, dir_location, full_filename, filesize);

    } else {
        printf("incorrect number of arguments given\n");
        exit(1);
    }

    return 0;
}