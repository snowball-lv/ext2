#pragma once

#define MAX_NAME 256
#define MAX_PATH 4096    

#define VFS_FILE 0x8000
#define VFS_DIR  0x4000
#define VFS_LINK 0xa000

#define VFS_MASK_FMT 0xf000

typedef int64_t Vnum;
typedef struct Vnode Vnode;
typedef struct Stat Stat;

typedef struct {
    Vnum vnum;
    char name[MAX_NAME];
} DirEnt;

struct Vnode {
    char name[MAX_NAME];
    void *device;
    Vnum vnum;
    int flags;
    int (*read)(Vnode *vn, void *dst, int off, int count);
    int (*write)(Vnode *vn, int off, int count, void *src);
    int (*find)(Vnode *parent, Vnode *dst, char *name);
    int (*readdir)(Vnode *parent, DirEnt *dst, int index);
    int (*create)(Vnode *parent, char *name, int isdir);
    int (*truncate)(Vnode *vn);
    int (*unlink)(Vnode *parent, char *name);
    int (*symlink)(Vnode *parent, char *name, char *value);
    int (*link)(Vnode *old, Vnode *newdir, char *newname);
    int (*stat)(Vnode *vn, Stat *dst);
};

struct Stat {
    uint32_t dev;
    uint32_t inum;
    uint32_t mode;
    uint32_t numlinks;
    uint32_t uid;
    uint32_t gid;
    uint32_t rdev;
    uint32_t size;
    uint32_t blocksz;
    uint32_t blocks;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
};

int vfsread(Vnode *vn, void *dst, int off, int count);
int vfswrite(Vnode *vn, int off, int count, void *src);
int vfsfind(Vnode *parent, Vnode *dst, char *name);
int vfsresolve(Vnode *root, Vnode *parent, Vnode *dst, char *path);
int vfsreaddir(Vnode *parent, DirEnt *dst, int index);
int vfscreate(Vnode *parent, char *path, int isdir);
int vfstruncate(Vnode *vn);
int vfsunlink(Vnode *parent, char *path);
int vfsmkdir(Vnode *parent, char *path);
int vfssymlink(Vnode *parent, char *path, char *value);
int vfslink(Vnode *old, Vnode *newdir, char *newname);
int vfsstat(Vnode *vn, Stat *dst);
