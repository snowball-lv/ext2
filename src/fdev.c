#include <stdio.h>
#include <string.h>
#include <ext2/vfs.h>
#include <errno.h>
#include <ext2/ext2.h>

static int fdevread(Vnode *vn, void *dst, int off, int count) {
    FILE *fp = vn->device;
    if (fseek(fp, off, SEEK_SET))
        return -1;
    clearerr(fp);
    int n = fread(dst, 1, count, fp);
    if (n == 0 && ferror(fp)) return -1;
    return n;
}

static int fdevwrite(Vnode *vn, int off, int count, void *src) {
    FILE *fp = vn->device;
    if (fseek(fp, off, SEEK_SET))
        return -1;
    clearerr(fp);
    int n = fwrite(src, 1, count, fp);
    if (n == 0 && ferror(fp)) return -1;
    return n;
}

int mkfdev(Vnode *dst, char *filename) {
    FILE *f = fopen(filename, "r+");
    if (!f) return -1;
    memset(dst, 0, sizeof(Vnode));
    // strcpy(dst->name, filename);
    dst->device = f;
    dst->read = fdevread;
    dst->write = fdevwrite;
    return 0;
}
