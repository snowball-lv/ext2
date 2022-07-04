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

int vfsresolve(Vnode *root, Vnode *parent, Vnode *dst, char *path) {
    char name[MAX_NAME];
    Vnode tmp = path[0] == '/' ? *root : *parent;
    *dst = tmp;
    while ((path = nextname(name, path))) {
        if (vfsfind(&tmp, dst, name))
            return -1;
        if ((dst->flags & VFS_LINK) == VFS_LINK) {
            char buf[1024];
            int len = vfsread(dst, buf, 0, sizeof(buf));
            if (len < 0) return -1;
            buf[len] = 0;
            return vfsresolve(root, &tmp, dst, buf);
        }
        tmp = *dst;
    }
    return 0;
}

int vfsreaddir(Vnode *parent, DirEnt *dst, int index) {
    if (!parent->readdir) return -1;
    return parent->readdir(parent, dst, index);
}

int vfscreate(Vnode *parent, char *path, int isdir) {
    char name[MAX_NAME];
    Vnode prev = *parent;
    Vnode tmp;
    while ((path = nextname(name, path))) {
        int dir = isdir || path[0] != 0;
        if (vfsfind(&prev, &tmp, name)) {
            if (!prev.create) return -1;
            if (prev.create(&prev, name, dir)) {
                printf("*** couldn't create [%s]\n", name);
                return -1;
            }
            if (vfsfind(&prev, &tmp, name)) {
                printf("*** couldn't find created [%s]\n", name);
                return -1;
            }
        }
        prev = tmp;
    }
    return 0;
}

int vfssymlink(Vnode *parent, char *path, char *value) {
    char name[MAX_NAME];
    Vnode prev = *parent;
    Vnode tmp;
    while ((path = nextname(name, path))) {
        int isdir = path[0] != 0;
        if (vfsfind(&prev, &tmp, name)) {
            if (isdir) {
                if (!prev.create) return -1;
                if (prev.create(&prev, name, isdir)) {
                    printf("*** couldn't create [%s]\n", name);
                    return -1;
                }
            }
            else {
                if (!prev.symlink) return -1;
                return prev.symlink(&prev, name, value);
            }
            if (vfsfind(&prev, &tmp, name)) {
                printf("*** couldn't find created [%s]\n", name);
                return -1;
            }
        }
        prev = tmp;
    }
    return 0;
}

int vfstruncate(Vnode *vn) {
    if (!vn->truncate) return -1;
    return vn->truncate(vn);
}

int vfsunlink(Vnode *parent, char *path) {
    char name[MAX_NAME];
    name[0] = 0;
    Vnode prev = *parent;
    Vnode tmp;
    while ((path = nextname(name, path))) {
        if (vfsfind(&prev, &tmp, name)) {
            printf("*** couldn't find [%s]\n", name);
            goto end;
        }
        if (path[0] == 0)
            goto found;
        prev = tmp;
    }
found:
    if (name[0]) {
        if (!prev.unlink) return -1;
        return prev.unlink(&prev, name);
    }
    printf("*** can't unlink root\n");
    return -1;
end:
    return -1;
}

int vfslink(Vnode *old, Vnode *newdir, char *newname) {
    if (!old->link) return -1;
    return old->link(old, newdir, newname);
}
