/*
* Programming Assignment 02: lsv1.1.0
* This is the source file of version 1.1.0
* Adds the -l option (long listing format) to version 1.0.0
* Usage:
*       $ lsv1.1.0
*       $ lsv1.1.0  -l
*       $ lsv1.1.0  -l  /home/kali/   /etc/
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

/* half a year in seconds (365.2425 days / 2), the same cut-off GNU ls uses to show time vs year */
#define SIX_MONTHS 15778476

extern int errno;

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void mode_to_str(mode_t mode, char *str);
char *user_name(uid_t uid);
char *group_name(gid_t gid);
int num_width(unsigned long n);
void time_to_str(time_t mtime, char *buf, size_t len);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    /* getopt() returns each option letter in turn, and -1 when no options are left */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        if (opt == 'l')
            long_flag = 1;
        else
        {
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            exit(1);
        }
    }

    /* optind is the index of the first argument that is not an option */
    if (optind == argc)
    {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
	    puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/*
* Long listing (-l). The directory is read twice:
*   Pass 1: find the widest value of each column and add up the blocks for "total".
*   Pass 2: print every entry, padding each column to the width found in pass 1.
*/
void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    long total_blocks = 0;
    int w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    int w_major = 0, w_minor = 0;       /* only used for device files */
    int len;

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* ---------- Pass 1: column widths and total ---------- */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        /* d_name is only the name, so join it with the directory to get a usable path */
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        /* lstat() so that a symbolic link is described itself, not the file it points to */
        if (lstat(path, &st) == -1)
            continue;

        total_blocks += st.st_blocks;

        len = num_width(st.st_nlink);
        if (len > w_links) w_links = len;

        len = strlen(user_name(st.st_uid));
        if (len > w_user) w_user = len;

        len = strlen(group_name(st.st_gid));
        if (len > w_group) w_group = len;

        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        {
            len = num_width(major(st.st_rdev));
            if (len > w_major) w_major = len;
            len = num_width(minor(st.st_rdev));
            if (len > w_minor) w_minor = len;
        }
        else
        {
            len = num_width(st.st_size);
            if (len > w_size) w_size = len;
        }
    }

    /* device files show "major, minor" in the size column, so it must be wide enough for that */
    if (w_major > 0 && w_major + 2 + w_minor > w_size)
        w_size = w_major + 2 + w_minor;

    /* st_blocks counts 512-byte blocks, but ls reports 1024-byte blocks */
    printf("total %ld\n", total_blocks / 2);

    /* ---------- Pass 2: print each entry ---------- */
    rewinddir(dp);          /* go back to the first entry of the directory */
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        char perms[11];
        char size[64];
        char mtime[32];

        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        mode_to_str(st.st_mode, perms);
        time_to_str(st.st_mtime, mtime, sizeof(mtime));

        /* size column: bytes, or "major, minor" device numbers for device files */
        if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
            snprintf(size, sizeof(size), "%*u, %*u",
                     w_major, major(st.st_rdev), w_minor, minor(st.st_rdev));
        else
            snprintf(size, sizeof(size), "%ld", (long)st.st_size);

        /* %*s right-aligns in the given width, %-*s left-aligns */
        printf("%s %*lu %-*s %-*s %*s %s %s",
               perms,
               w_links, (unsigned long)st.st_nlink,
               w_user, user_name(st.st_uid),
               w_group, group_name(st.st_gid),
               w_size, size,
               mtime,
               entry->d_name);

        /* for a symbolic link, also show where it points: "name -> target" */
        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';       /* readlink() does not add the '\0' itself */
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/*
* Converts st_mode into the 10-character string shown by ls -l, e.g. "drwxr-xr-x".
* str must have room for 11 chars (10 + '\0').
*/
void mode_to_str(mode_t mode, char *str)
{
    /* character 0: file type */
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    /* characters 1-9: read/write/execute for owner, group and others */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* special bits replace the execute position: lowercase if x is also set, uppercase if not */
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/* Returns the user name for uid, or the number itself if the user does not exist */
char *user_name(uid_t uid)
{
    static char num[32];
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        return pw->pw_name;
    snprintf(num, sizeof(num), "%u", uid);
    return num;
}

/* Returns the group name for gid, or the number itself if the group does not exist */
char *group_name(gid_t gid)
{
    static char num[32];
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        return gr->gr_name;
    snprintf(num, sizeof(num), "%u", gid);
    return num;
}

/* Returns how many characters n takes when printed, e.g. 4096 -> 4 */
int num_width(unsigned long n)
{
    char buf[32];
    return snprintf(buf, sizeof(buf), "%lu", n);    /* snprintf returns the length it wrote */
}

/*
* ctime() returns a fixed-layout string like "Wed Jun 30 21:49:08 1993\n".
* Like ls, show "Jun 30 21:49" for recent files and "Jun 30  1993" for files
* older than six months (or dated in the future).
*/
void time_to_str(time_t mtime, char *buf, size_t len)
{
    char *t = ctime(&mtime);
    time_t now = time(NULL);

    if (now - mtime > SIX_MONTHS || mtime > now)
        snprintf(buf, len, "%.7s %.4s", t + 4, t + 20);   /* "Jun 30 " + " " + "1993" */
    else
        snprintf(buf, len, "%.12s", t + 4);               /* "Jun 30 21:49" */
}
