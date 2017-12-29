#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

void syscall_init (void);
void validate_pointer(const void* pointer, void *esp);
void clear_files(void);

void syscall_exit(const int exit_type);


/* locks the file system to avoid data races */
struct lock lock_filesystem;
struct lock lock_mmap;

#endif /* userprog/syscall.h */
