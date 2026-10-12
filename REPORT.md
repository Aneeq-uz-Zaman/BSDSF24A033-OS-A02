# OS Programming Assignment 02 – Report

**Name:** Muhammad Aneeq-uz-Zaman
**Roll No:** BSDSF24A033

---

## Feature 2: ls-v1.1.0 – Complete Long Listing Format

### Q1. What is the crucial difference between the `stat()` and `lstat()` system calls? In the context of the `ls` command, when is it more appropriate to use `lstat()`?

Both calls fill a `struct stat` with a file's metadata (type, permissions, link count, owner, group, size, timestamps). The difference is in how they treat a **symbolic link**:

- `stat()` **follows** the link and returns information about the file the link points to (the target).
- `lstat()` does **not** follow the link. It returns information about the link itself.

For a normal file or directory both calls give the same result.

`ls -l` should use `lstat()`, because `ls` is supposed to show what is actually inside the directory. A symbolic link must be shown as a link (type `l`, permissions `lrwxrwxrwx`, size equal to the length of the target path) followed by `-> target`. With `stat()` the link would look like the target file (for example, a regular file with the target's size), so the user could not tell it is a link. Also, for a broken link (target does not exist) `stat()` fails, but `lstat()` still works, so the link can still be listed.

In my code, `do_ls_long()` calls `lstat()` on every entry, and if the entry is a link (`S_ISLNK`), it calls `readlink()` to print the `-> target` part.

### Q2. The `st_mode` field in `struct stat` is an integer that contains both the file type and the permission bits. Explain how you can use bitwise operators (like `&`) and predefined macros (like `S_IFDIR` or `S_IRUSR`) to extract this information.

`st_mode` packs several pieces of information into different bits of one integer:

```
 bits 15-12 | bit 11 | bit 10 | bit 9  | bits 8-6 | bits 5-3 | bits 2-0
 file type  | setuid | setgid | sticky |  owner   |  group   |  others
            |        |        |        |   rwx    |   rwx    |   rwx
```

Each macro is a mask with only the relevant bits set. The bitwise AND (`&`) keeps only those bits and clears all others, so we can test one piece of information at a time.

**Permission bits.** Each permission is a single bit, so `mode & mask` is non-zero if that permission is set:

```c
str[1] = (mode & S_IRUSR) ? 'r' : '-';   /* S_IRUSR = 0400, owner read    */
str[2] = (mode & S_IWUSR) ? 'w' : '-';   /* S_IWUSR = 0200, owner write   */
str[3] = (mode & S_IXUSR) ? 'x' : '-';   /* S_IXUSR = 0100, owner execute */
```

and the same for the group (`S_IRGRP`, `S_IWGRP`, `S_IXGRP`) and others (`S_IROTH`, `S_IWOTH`, `S_IXOTH`).

**File type.** The type uses several bits together, so a single `&` with `S_IFDIR` is not enough. We first keep only the type bits with the mask `S_IFMT` (0170000) and then compare the result:

```c
if ((mode & S_IFMT) == S_IFDIR)   /* directory */
```

The macros `S_ISDIR(m)`, `S_ISLNK(m)`, `S_ISREG(m)`, `S_ISCHR(m)`, `S_ISBLK(m)`, `S_ISFIFO(m)` and `S_ISSOCK(m)` do exactly this comparison, and that is what my `mode_to_str()` uses to choose the first character (`d`, `l`, `-`, `c`, `b`, `p`, `s`).

**Special bits.** `S_ISUID` (setuid), `S_ISGID` (setgid) and `S_ISVTX` (sticky) are tested the same way. `ls` shows them in the execute position: lowercase `s`/`t` if the execute bit is also set, uppercase `S`/`T` if it is not:

```c
if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
```

For example, `/usr/bin/passwd` has mode `0104755` → type bits = regular file, setuid set, owner `rwx` → shown as `-rwsr-xr-x`.

### Implementation notes

- `-l` is parsed with `getopt()`. With `-l`, `main()` calls the new function `do_ls_long()`; otherwise it calls the original `do_ls()`.
- `do_ls_long()` reads the directory twice. Pass 1 finds the widest value of each column and adds up `st_blocks` for the `total` line (divided by 2, because `st_blocks` counts 512-byte blocks and `ls` shows 1024-byte blocks). `rewinddir()` then goes back to the start, and pass 2 prints each entry padded to those widths.
- Owner and group names come from `getpwuid()` and `getgrgid()`. If no name exists, the numeric ID is printed instead.
- The time comes from `ctime()`. Like `ls`, files modified in the last six months show `Mon DD HH:MM`, and older (or future) files show `Mon DD  YYYY`.
- Device files show `major, minor` in the size column instead of a size.
- The output was compared with the real `ls -lU` (unsorted, since sorting comes in v1.4.0) on many directories and matched exactly. Known differences: GNU `ls` adds a `+` after the permissions of files that have ACLs, which this version does not detect; and in `/dev`, GNU `ls` prints normal files one column narrower than device files, while this version keeps all columns aligned.

---

## Feature 3: ls-v1.2.0 – Column Display (Down Then Across)

### Q1. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient for this task?

In "down then across" order the names fill the **first column from top to bottom**, then the second column, and so on:

```
a  d  g
b  e  h
c  f
```

The steps in my `do_ls()` / `print_columns()` are:

1. **Read all names first** into a dynamic array (`char **names`, grown with `realloc()`, each name copied with `strdup()`), and remember the length of the longest name.
2. **Work out the layout:**
   - column width = longest name + 2 spaces
   - `cols = terminal_width / column_width` (at least 1)
   - `rows = ceil(count / cols)`, computed as `(count + cols - 1) / cols`
3. **Print row by row.** The terminal can only print left to right, top to bottom, so the outer loop goes over rows and the inner loop over columns. The item at row `r`, column `c` is `names[r + c * rows]`: moving down one row adds 1 to the index, and moving right one column skips a whole column, i.e. adds `rows`. Each name is padded with spaces to the column width (`%-*s`), except the last name in a row.

A single loop (`for i = 0 .. count-1`) prints the names in the order `0, 1, 2, 3, ...`. On screen that fills each **row** first (across then down), which is the `-x` style, not the `ls` default. To print down then across, the names that sit next to each other on a screen line are **not** next to each other in the array (they are `rows` apart), so we need to know the number of rows first and then jump through the array with `r + c * rows`. Also, a single loop cannot know the column width or the number of rows without first having seen every name.

### Q2. What is the purpose of the `ioctl` system call in this context? What would be the limitations of your program if you only used a fixed-width fallback (e.g., 80 columns) instead of detecting the terminal size?

`ioctl()` ("input/output control") sends device-specific requests to a file descriptor. With the request `TIOCGWINSZ` ("Terminal IOCtl Get WINdow SiZe") on `STDOUT_FILENO`, the terminal driver fills a `struct winsize` whose `ws_col` field is the current width of the terminal in characters (and `ws_row` the height). My `get_terminal_width()` uses this to calculate how many columns fit:

```c
struct winsize w;
if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
    return DEFAULT_WIDTH;          /* 80 */
return w.ws_col;
```

If the output is not a terminal (for example redirected to a file or piped to another command), `ioctl()` fails and the program falls back to 80 columns.

Limitations of using only a fixed width of 80:

- **Wide terminal:** the output uses only the left 80 characters, so it shows fewer columns and many more rows than needed, wasting screen space.
- **Narrow terminal** (less than 80): lines are longer than the screen, so the terminal wraps them onto the next line and the columns become broken and unreadable.
- **Resizing:** the layout never adapts when the user resizes the window; with `ioctl()` the next run automatically uses the new width.

---

## Feature 4: ls-v1.3.0 – Horizontal Column Display (-x Option)

### Q1. Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?

The **"down then across"** (default) logic needs more pre-calculation. Before printing the first line it must know:

- the column width (longest name + 2),
- how many columns fit (`terminal_width / column_width`),
- and, most importantly, **how many rows** there will be: `rows = ceil(count / cols)`.

The number of rows is needed because the names that appear side by side on one screen line are not next to each other in the array: the item at row `r`, column `c` is `names[r + c * rows]`. So the total number of files must be known and the whole layout calculated before any output, and the code uses two nested loops (rows, then columns) with index arithmetic.

The **"across"** (`-x`) logic is simpler. Names are printed in the same order as the array, left to right, so one loop is enough. It only needs the column width (from the longest name) and the terminal width; it keeps a running position `pos` on the current line, and when `pos + column_width` would go past the terminal width, it prints `\n` and resets `pos` to 0. It never needs the number of rows or columns in advance.

### Q2. Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?

I used a single integer variable `mode` in `main()` together with three named constants:

```c
#define MODE_DEFAULT    0    /* no option: down then across */
#define MODE_LONG       1    /* -l */
#define MODE_HORIZONTAL 2    /* -x */
```

1. `mode` starts as `MODE_DEFAULT`. The `getopt(argc, argv, "lx")` loop sets it to `MODE_LONG` for `-l` and `MODE_HORIZONTAL` for `-x`. Because each option overwrites the variable, if both are given the last one wins, the same as the real `ls`.
2. `main()` passes `mode` to `do_ls(dir, mode)`.
3. `do_ls()` always does the same first step for every mode: read all the names into the dynamic array and find the longest name.
4. Then it checks the mode and calls the matching display function:

```c
if (mode == MODE_LONG)
    print_long(dir, names, count);
else if (mode == MODE_HORIZONTAL)
    print_horizontal(names, count, max_len);
else
    print_columns(names, count, max_len);
```

Finally it frees the array. Because reading is separated from printing, each display function only formats an array it is given, and new modes (or sorting in the next version) can be added in one place. For this version the long listing was changed from reading the directory itself (with `rewinddir()`) to looping over the same array, so all three modes now share the same list of names.
