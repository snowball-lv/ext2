#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ext2/vfs.h>
#include <ext2/ext2.h>

#define STATE_VALID 1
#define STATE_ERROR 2

#define ERRORS_CONTINUE  1
#define ERRORS_READ_ONLY 2
#define ERRORS_PANIC     3

#define CREATOR_LINUX   0
#define CREATOR_HURD    1
#define CREATOR_MASIX   2
#define CREATOR_FREEBSD 3
#define CREATOR_LITES   4

#define REV_0 0
#define REV_1 1

#define ROOT_INUM 2

#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFLNK 0xa000

static int hasformat(int mode, int format) {
    return (mode & format) == format;
}

static uint64_t inodesize(Inode *inode) {
    return inode->size;
}

static void setinodesize(Inode *inode, uint64_t size) {
    inode->size = size;
}

static int readdev(Ext2 *ext2, void *dst, int off, int count) {
    int n = vfsread(ext2->bdev, dst, off, count);
    return n < 0 ? n : 0;
}

static int readblock(Ext2 *ext2, uint32_t block, void *dst) {
    if (readdev(ext2, dst, block * ext2->blocksz, ext2->blocksz))
        return -1;
    return 0;
}

static void *allocmemblock(Ext2 *ext) {
    return malloc(ext->blocksz);
}

static void freememblock(void *block) {
    free(block);
}

static int readsb(Ext2 *ext2) {
    if (readdev(ext2, &ext2->sb, 1024, sizeof(Superblock)))
        return -1;
    // // TODO: convert to host endianness
    ext2->blocksz = 1024 << ext2->sb.blockszshift;
    ext2->inodesz = ext2->sb.revmajor > REV_0 ? ext2->sb.inodesz : 128;
    ext2->numgroups = (ext2->sb.numblocks + ext2->sb.blockspergroup - 1)
            / ext2->sb.blockspergroup;
    ext2->ppb = ext2->blocksz / 4;
    return 0;
}

static uint32_t grouptab(Ext2 *ext2) {
    return ext2->blocksz == 1024 ? 2 : 1;
}

static int readgroup(Ext2 *ext2, Group *dst, int i) {
    if (i >= ext2->numgroups)
        return -1;
    char *tmp = allocmemblock(ext2);
    int off = i * sizeof(Group);
    int block = grouptab(ext2) + off / ext2->blocksz;
    if (readblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    memcpy(dst, &tmp[off % ext2->blocksz], sizeof(Group));
    freememblock(tmp);
    return 0;
}

static int readinode(Ext2 *ext2, Inode *dst, uint32_t inum) {
    int gnum = (inum - 1) / ext2->sb.inodespergroup;
    Group g;
    if (readgroup(ext2, &g, gnum))
        return -1;
    int idx = (inum - 1) % ext2->sb.inodespergroup;
    int block = g.indoetab + (idx * ext2->inodesz) / ext2->blocksz;
    int blockidx = idx % (ext2->blocksz / ext2->inodesz);
    uint8_t *tmp = allocmemblock(ext2);
    if (readblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    memcpy(dst, &tmp[blockidx * ext2->inodesz], ext2->inodesz);
    freememblock(tmp);
    return 0;
}

static int writedev(Ext2 *ext2, int off, int count, void *src) {
    int n = vfswrite(ext2->bdev, off, count, src);
    if (n != count)
        printf("*** wrote %i of %i\n", n, count);
    return n < 0 ? n : 0;
}

static int writeblock(Ext2 *ext2, uint32_t block, void *src) {
    return writedev(ext2, block * ext2->blocksz, ext2->blocksz, src);
}

static int flushsb(Ext2 *ext2) {
    return writedev(ext2, 1024, sizeof(Superblock), &ext2->sb);
}

static int flushgroup(Ext2 *ext2, int i, Group *src) {
    if (i >= ext2->numgroups)
        return -1;
    char *tmp = allocmemblock(ext2);
    int pos = i * sizeof(Group);
    int block = grouptab(ext2) + pos / ext2->blocksz;
    if (readblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    memcpy(&tmp[pos % ext2->blocksz], src, sizeof(Group));
    if (writeblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    freememblock(tmp);
    return 0;
}

static int testbit(void *buf, int num) {
    return ((char *)buf)[num / 8] & (1 << (num % 8));
}

static void setbit(void *buf, int num) {
    ((char *)buf)[num / 8] |= 1 << (num % 8);
}

static int allocblock(Ext2 *ext2) {
    uint8_t *bitmap = allocmemblock(ext2);
    int block = -1;
    for (int gi = 0; gi < ext2->numgroups; gi++) {
        Group g;
        if (readgroup(ext2, &g, gi)) goto end;
        if (!g.freeblocks) continue;
        if (readblock(ext2, g.blockbitmap, bitmap)) goto end;
        for (int i = 0; i < ext2->sb.blockspergroup; i++) {
            if (testbit(bitmap, i)) continue;
            // set changes
            ext2->sb.numfreeblocks--;
            g.freeblocks--;
            setbit(bitmap, i);
            // flush changes
            if (flushsb(ext2)) goto end;
            if (flushgroup(ext2, gi, &g)) goto end;
            if (writeblock(ext2, g.blockbitmap, bitmap)) goto end;
            // set found block number
            block = ext2->sb.firstblock + gi * ext2->sb.blockspergroup + i;
            goto end;
        }
    }
end:
    freememblock(bitmap);
    return block;
}

static void freeblock(Ext2 *ext2, int block) {
    // TODO
}

static int writeinode(Ext2 *ext2, uint32_t inum, Inode *src) {
    int gnum = (inum - 1) / ext2->sb.inodespergroup;
    Group g;
    if (readgroup(ext2, &g, gnum))
        return -1;
    int idx = (inum - 1) % ext2->sb.inodespergroup;
    int block = g.indoetab + (idx * ext2->inodesz) / ext2->blocksz;
    int blockidx = idx % (ext2->blocksz / ext2->inodesz);
    uint8_t *tmp = allocmemblock(ext2);
    if (readblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    memcpy(&tmp[blockidx * ext2->inodesz], src, ext2->inodesz);
    if (writeblock(ext2, block, tmp)) {
        freememblock(tmp);
        return -1;
    }
    freememblock(tmp);
    return 0;
}

static int getinodeblock(Ext2 *ext2, uint32_t inum, int idx, int create) {
    if (idx < 0) return -1;
    Inode inode;
    if (readinode(ext2, &inode, inum)) return -1;
    if (idx * ext2->blocksz >= inodesize(&inode) && !create) return -1;
    if (idx < 12) {
        int block = inode.blocks[idx];
        if (!block && create) {
            block = allocblock(ext2);
            if (block < 0) return -1;
            inode.blocks[idx] = block;
            if (writeinode(ext2, inum, &inode)) {
                freeblock(ext2, block);
                return -1;
            }
        }
        else if (!block) {
            return -1;
        }
        return block;
    }
    printf("*** file too big\n");
    return -1;
}

static int ext2read(Vnode *vn, void *dst, int off, int count) {
    Ext2 *ext2 = vn->device;
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    // if (!(inode.mode & EXT2_S_IFREG))
    //     return -1;
    uint64_t isz = inodesize(&inode);
    if (off >= isz) return 0;
    if (count <= 0) return 0;
    count = off + count < isz ? count : isz - off;
    int end = off + count;
    char *tmp = allocmemblock(ext2);
    while (off < end) {
        int relblock = off / ext2->blocksz;
        int absblock = getinodeblock(ext2, vn->vnum, relblock, 0);
        if (absblock < 0) goto error;
        if (readblock(ext2, absblock, tmp)) goto error;
        int blockoff = off % ext2->blocksz;
        int blockrem = ext2->blocksz - blockoff;
        int len = blockrem < end - off ? blockrem : end - off;
        memcpy(dst, &tmp[blockoff], len);
        off += len;
        dst += len;
    }
    freememblock(tmp);
    return count;
error:
    freememblock(tmp);
    return -1;
}

static int ext2truncate(Vnode *vn) {
    Ext2 *ext2 = vn->device;
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    setinodesize(&inode, 0);
    inode.sectors = 0;
    memset(inode.blocks, 0, sizeof(inode.blocks));
    if (writeinode(ext2, vn->vnum, &inode))
        return -1;
    return 0;
}

static int ext2write(Vnode *vn, int off, int count, void *src) {
    Ext2 *ext2 = vn->device;
    int blockoff = off % ext2->blocksz;
    int blockrem = ext2->blocksz - blockoff;
    count = blockrem < count ? blockrem : count;
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    // if (!(inode.mode & EXT2_S_IFREG)) {
    //     printf("*** [%s] not a regular file\n", vn->name);
    //     return -1;
    // }
    int relblock = off / ext2->blocksz;
    int absblock = getinodeblock(ext2, vn->vnum, relblock, 1);
    if (absblock < 0) {
        printf("*** block #%i doesn't exist\n", relblock);
        return -1;
    }
    // refresh inode
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    char *tmp = allocmemblock(ext2);
    if (readblock(ext2, absblock, tmp)) {
        freememblock(tmp);
        return -1;
    }
    memcpy(&tmp[blockoff], src, count);
    if (writeblock(ext2, absblock, tmp)) {
        freememblock(tmp);
        return -1;
    }
    freememblock(tmp);
    if (off + count > inodesize(&inode)) {
        setinodesize(&inode, off + count);
        if (writeinode(ext2, vn->vnum, &inode))
            return -1;
    }
    return count;
}

static int fillvnode(Ext2 *ext2, Vnode *dst, uint32_t inum);

static int ext2readdir(Vnode *parent, DirEnt *dst, int index) {
    if (!(parent->flags & VFS_DIR))
        return -1;
    struct {
        Ext2DirEnt de;
        char namebuf[MAX_NAME];
    } s;
    int i = 0;
    int off = 0;
    int r;
    while ((r = ext2read(parent, &s.de, off, sizeof(Ext2DirEnt) + MAX_NAME))) {
        if (s.de.inum && i == index) {
            memcpy(dst->name, s.de.name, s.de.namelen);
            dst->name[s.de.namelen] = 0;
            dst->vnum = s.de.inum;
            return 0;
        }
        off += s.de.reclen;
        if (s.de.inum)
            i++;
    }
    return -1;
}

static int ext2find(Vnode *parent, Vnode *dst, char *name) {
    DirEnt de;
    int i = 0;
    while (ext2readdir(parent, &de, i) == 0) {
        if (strcmp(de.name, name) == 0) {
            Ext2 *ext2 = parent->device;
            fillvnode(ext2, dst, de.vnum);
            return 0;
        }
        i++;
    }
    return -1;
}

static uint32_t allocinode(Ext2 *ext2) {
    uint8_t *bitmap = allocmemblock(ext2);
    uint32_t inum = 0;
    for (int gi = 0; gi < ext2->numgroups; gi++) {
        Group g;
        if (readgroup(ext2, &g, gi))
            goto end;
        if (!g.freeinodes) continue;
        if (readblock(ext2, g.inodebitmap, bitmap))
            goto end;
        for (int i = 0; i < ext2->sb.inodespergroup; i++) {
            int byte = i / 8;
            int bit = i % 8;
            if ((!(bitmap[byte] & (1 << bit)))) {
                ext2->sb.numfreeinodes--;
                g.freeinodes--;
                bitmap[byte] |= 1 << bit;
                if (flushsb(ext2)) goto end;
                if (flushgroup(ext2, gi, &g)) goto end;
                if (writeblock(ext2, g.inodebitmap, bitmap)) goto end;
                inum = gi * ext2->sb.inodespergroup + i + 1;
                goto end;
            }
        }
    }
end:
    freememblock(bitmap);
    return inum;
}

static void freeinode(Ext2 *ext2, uint32_t inum) {
    printf("freeing inode %u\n", inum);
    // TODO
}

static void fillinode(Inode *dst) {
    memset(dst, 0, sizeof(Inode));
    dst->mode = 0;
    dst->uid = 0;
    dst->size = 0;
    dst->atime = 0;
    dst->ctime = 0;
    dst->mtime = 0;
    dst->dtime = 0;
    dst->gid = 0;
    dst->numlinks = 0;
    dst->sectors = 0;
    dst->flags = 0;
    dst->osval1 = 0;
    // dst->blocks = 0;
    dst->generation = 0;
    dst->fileacl = 0;
    dst->diracl = 0;
    dst->faddr = 0;
    // dst->osval2 = 0;
}

static int inclinks(Ext2 *ext2, uint32_t inum) {
    Inode inode;
    if (readinode(ext2, &inode, inum))
        return -1;
    inode.numlinks++;
    if (writeinode(ext2, inum, &inode))
        return -1;
    return 0;
}

static int mkentry(Vnode *parent, char *name, uint32_t inum) {
    Ext2 *ext2 = parent->device;
    Inode inode;
    if (readinode(ext2, &inode, parent->vnum))
        return -1;
    if (!hasformat(inode.mode, EXT2_S_IFDIR))
        return -1;
    int namelen = strlen(name);
    int reclen = sizeof(Ext2DirEnt) + namelen;
    struct {
        Ext2DirEnt de;
        char namebuf[MAX_NAME];
    } s;
    s.de.inum = inum;
    s.de.reclen = reclen;
    s.de.namelen = namelen;
    s.de.filetype = 0;
    memcpy(s.de.name, name, namelen);
    int w = ext2write(parent, inodesize(&inode), reclen, &s.de);
    if (w != reclen) {
        printf("*** wrote %i of %i\n", w, reclen);
        return -1;
    }
    if (inclinks(ext2, inum))
        return -1;
    return 0;
}

static int fillvnode(Ext2 *ext2, Vnode *dst, uint32_t inum);

static int ext2create(Vnode *parent, char *name, int isdir) {
    if (!(parent->flags & VFS_DIR)) {
        printf("*** parent not a dir\n");
        return -1;
    }
    Ext2 *ext2 = parent->device;
    uint32_t inum = allocinode(ext2);
    if (!inum) return -1;
    Inode inode;
    if (readinode(ext2, &inode, inum)) {
        freeinode(ext2, inum);
        return -1;
    }
    fillinode(&inode);
    inode.mode = isdir ? EXT2_S_IFDIR : EXT2_S_IFREG;
    inode.numlinks = 0;
    if (writeinode(ext2, inum, &inode)) {
        freeinode(ext2, inum);
        return -1;
    }
    if (mkentry(parent, name, inum)) {
        printf("*** couldn't make entry\n");
        freeinode(ext2, inum);
        return -1;
    }
    if (isdir) {
        Vnode vn;
        if (fillvnode(ext2, &vn, inum))
            return -1;
        mkentry(&vn, ".", inum);
        mkentry(&vn, "..", parent->vnum);
    }
    return 0;
}

int ext2unlink(Vnode *parent, char *name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("*** can't unlink special entries\n");
        return -1;
    }
    int rv = -1;
    Ext2 *ext2 = parent->device;
    Inode inode;
    if (readinode(ext2, &inode, parent->vnum))
        return -1;
    int off = 0;
    int size = inodesize(&inode);
    char *tmp = allocmemblock(ext2);
    int namelen = strlen(name);
    Ext2DirEnt *prev = 0;
    Ext2DirEnt *target = 0;
    int absblock = 0;
    int boff = 0;
    while (off < size) {
        int relblock = off / ext2->blocksz;
        absblock = getinodeblock(ext2, parent->vnum, relblock, 0);
        if (absblock < 0) goto end;
        if (readblock(ext2, absblock, tmp)) goto end;
        prev = 0;
        target = 0;
        boff = 0;
        while (boff < ext2->blocksz) {
            Ext2DirEnt *de = (void *)&tmp[boff];
            if (de->namelen == namelen) {
                if (strncmp(de->name, name, namelen) == 0) {
                    target = de;
                    goto found;
                }
            }
            prev = de;
            boff += de->reclen;
        }
        off += ext2->blocksz;
    }
found:;
    Inode tinode;
    if (readinode(ext2, &tinode, target->inum))
        goto end;
    tinode.numlinks--;
    if (writeinode(ext2, target->inum, &tinode))
        goto end;
    if (tinode.numlinks == 0) { 
        freeinode(ext2, target->inum);
    }
    if (prev) {
        prev->reclen += target->reclen;
        if (writeblock(ext2, absblock, tmp)) goto end;
        rv = 0;
    }
    else {
        target->namelen = 0;
        target->inum = 0;
        if (writeblock(ext2, absblock, tmp)) goto end;
        rv = 0;
    }
end:
    freememblock(ext2);
    return rv;
}

int ext2symlink(Vnode *parent, char *name, char *value) {
    if (ext2create(parent, name, 0))
        return -1;
    Vnode vn;
    Inode i;
    if (ext2find(parent, &vn, name))
        return -1;
    if (readinode(parent->device, &i, vn.vnum))
        return -1;
    i.mode = EXT2_S_IFLNK;
    if (writeinode(parent->device, vn.vnum, &i))
        return -1;
    int len = strlen(value);
    if (ext2write(&vn, 0, len, value) != len)
        return -1;
    return 0;
}

int ext2link(Vnode *old, Vnode *newdir, char *newname) {
    return mkentry(newdir, newname, old->vnum);
}

static int fillvnode(Ext2 *ext2, Vnode *dst, uint32_t inum) {
    Inode inode;
    if (readinode(ext2, &inode, inum))
        return -1;
    memset(dst, 0, sizeof(Vnode));
    // sprintf(dst->name, "[%i]", inum);
    dst->device = ext2;
    dst->vnum = inum;
    dst->flags = inode.mode;
    dst->read = ext2read;
    dst->write = ext2write;
    dst->find = ext2find;
    dst->readdir = ext2readdir;
    dst->create = ext2create;
    dst->truncate = ext2truncate;
    dst->unlink = ext2unlink;
    dst->symlink = ext2symlink;
    dst->link = ext2link;
    return 0;
}

int mkext2(Vnode *dst, Vnode *bdev) {
    Ext2 *ext2 = malloc(sizeof(Ext2));
    memset(ext2, 0, sizeof(Ext2));
    ext2->bdev = bdev;
    if (readsb(ext2))
        return -1;
    if (fillvnode(ext2, dst, 2))
        return -1;
    strcpy(dst->name, "[root]");
    return 0;
}