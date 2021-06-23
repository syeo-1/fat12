int get_num_files_on_disk(char* p);
int first_logic_cluster_zero_or_one(char* p, int i);
void get_full_filename(char* p, char* filename, int dir_start, char data_type);
int get_dir_or_file_size(char* p, int dir_start, char data_type);
int get_fat_entry_value(char* p, int entry_number);
char get_dir_or_file(char* p, int dir_start);
void get_dirname(char *p, char* calling_dirname, int dir_start);
int get_free_size_on_disk(char* p);