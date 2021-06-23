#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common_functions.h"

void get_file_creation(char* p, int dir_start) {
    int time, date;
    int hours, minutes, day, month_num, year;

    const char * month_abbrevs[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    time = *(unsigned short *)(p + dir_start + 14);
    date = *(unsigned short *)(p + dir_start + 16);

    year = ((date & 0xfe00) >> 9) + 1980;
    month_num = (date & 0x1e0) >> 5;
    day = date & 0x1f;
    hours = (time & 0xf800) >> 11;
    minutes = (time & 0x7e0) >> 5;

    printf("%s %2d %d %02d:%02d\n", month_abbrevs[month_num-1], day, year, hours, minutes);
}

void print_single_dir_info(char* p, int dir_start, int dir_end) {
    for (int i = dir_start ; i < dir_end ; i+=32) {
        char data_type = get_dir_or_file(p, i);
        
        if(data_type == '\0') {
            continue;
        }
        char dirname[8];

        if (data_type == 'D') {
            get_dirname(p, dirname, i);
            if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {// ignore current and parent dir
                continue;
            }
        }

        printf("%c ", data_type);
        printf("%*d ", 10, get_dir_or_file_size(p, i, data_type));
        if (data_type == 'D') {
            printf("%*s ", 20, dirname);
        } else if (data_type == 'F') {
            char filename[12];
            get_full_filename(p, filename, i, 'F');
            printf("%*s ", 20, filename);
        }
        
        get_file_creation(p, i);

    }
}

void print_all_dir_info(char* p, char* dir_name, int dir_start, int dir_end) {
    //if it's the root directory, go through entire root directory
    if (strcmp(dir_name, "Root") == 0) {
        printf("Root\n");
        printf("==============\n");

        print_single_dir_info(p, dir_start, dir_end);

        
        for(int i = dir_start ; i < dir_end ; i+=32) { // recurse on any subdirectories in root directory
            char data_type = get_dir_or_file(p, i);
            if (data_type == 'D') { // recurse
                unsigned short first_logic_cluster = p[i+26];
                int physical_cluster_num = first_logic_cluster+31;
                char cur_dirname[8];
                get_dirname(p, cur_dirname, i);
                int subdir_start = physical_cluster_num*512;
                int subdir_end = subdir_start+512;
                print_all_dir_info(p, cur_dirname, subdir_start, subdir_end);
            }
        }
    } else { // for recursing on subdirectories
        printf("/%s\n", dir_name);
        printf("==============\n");

        print_single_dir_info(p, dir_start, dir_end);

        for(int i = dir_start ; i < dir_end ; i+=32) { // recurse on subdirectories in current subdirectory
            char data_type = get_dir_or_file(p, i);
            if (data_type == 'D') { // recurse
                unsigned short first_logic_cluster = p[i+26];
                int physical_cluster_num = first_logic_cluster+31;
                char cur_dir_name[8];
                get_dirname(p, cur_dir_name, i);
                if (strcmp(cur_dir_name, ".") == 0 || strcmp(cur_dir_name, "..") == 0) {// don't recurse on current or parent dir
                    continue;
                }
                char combined_dirname[strlen(dir_name)+strlen(cur_dir_name)+2];
                combined_dirname[0] = '\0';
                strncat(combined_dirname, dir_name, strlen(dir_name));
                strncat(combined_dirname, "/", 1);
                strncat(combined_dirname, cur_dir_name, strlen(cur_dir_name));
                int subdir_start = physical_cluster_num*512;
                int subdir_end = subdir_start+512;
                print_all_dir_info(p, combined_dirname, subdir_start, subdir_end);
            } 
        }
        
    }

}

int main(int argc, char *argv[]) {
    int fd;
	struct stat sb;

    if (argc == 2) {
        fd = open(argv[1], O_RDWR);
        fstat(fd, &sb);

        char * p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            printf("Error: failed to map memory\n");
            exit(1);
        }
        int root_dir = 19 * 512;
        int data_area = 33 * 512;

        // format example
        print_all_dir_info(p, "Root", root_dir, data_area);

    } else {
        printf("incorrect number of arguments given\n");
        exit(1);
    }

    return 0;
}