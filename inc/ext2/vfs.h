#pragma once

#define MAX_NAME 256

#define VFS_FILE 0x8000
#define VFS_DIR  0x4000

typedef int64_t Vnum;
typedef struct Vnode Vnode;

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
};

int vfsread(Vnode *vn, void *dst, int off, int count);
int vfswrite(Vnode *vn, int off, int count, void *src);
int vfsfind(Vnode *parent, Vnode *dst, char *name);
int vfsresolve(Vnode *parent, Vnode *dst, char *path);
int vfsreaddir(Vnode *parent, DirEnt *dst, int index);
int vfscreate(Vnode *parent, char *path, int isdir);
int vfstruncate(Vnode *vn);
int vfsunlink(Vnode *parent, char *name);
int vfsmkdir(Vnode *parent, char *path);
