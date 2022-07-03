#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ext2/vfs.h>

int vfsread(Vnode *vn, void *dst, int off, int count) {
    if (!vn->read) return -1;
    return vn->read(vn, dst, off, count);
}

int vfswrite(Vnode *vn, int off, int count, void *src) {
    if (!vn->write) return -1;
    return vn->write(vn, off, count, src);
}

static char *nextname(char *dst, char *path) {
    if (path[0] == '/') path++;
    if (path[0] == 0) return 0;
    char *delim = strchr(path, '/');
    int len = delim ? delim - path : strlen(path);
    memcpy(dst, path, len);
    dst[len] = 0;
    return path + len;
}

int vfsfind(Vnode *parent, Vnode *dst, char *name) {
    if (!parent->find) return -1;
    return parent->find(parent, dst, name);
}

int vfsresolve(Vnode *parent, Vnode *dst, char *path) {
    char name[MAX_NAME];
    Vnode tmp = *parent;
    *dst = *parent;
    while ((path = nextname(name, path))) {
        if (vfsfind(&tmp, dst, name))
            return -1;
        tmp = *dst;
    }
    return 0;
}

int vfsreaddir(Vnode *parent, DirEnt *dst, int index) {
    if (!parent->readdir) return -1;
    return parent->readdir(parent, dst, index);
}

int vfscreate(Vnode *parent, char *path) {
    char name[MAX_NAME];
    Vnode prev = *parent;
    Vnode tmp;
    while ((path = nextname(name, path))) {
        if (vfsfind(&prev, &tmp, name)) {
            printf("creating [%s]\n", name);
            if (!prev.create) return -1;
            return prev.create(&prev, name);
        }
        prev = tmp;
    }
    return 0;
}

int vfstruncate(Vnode *vn) {
    if (!vn->truncate) return -1;
    return vn->truncate(vn);
}

int vfsunlink(Vnode *parent, char *name) {
    if (!parent->unlink) return -1;
    return parent->unlink(parent, name);
}
