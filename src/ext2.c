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
        if (readgroup(ext2, &g, gi)) {
            freememblock(bitmap);
            return -1;
        }
        if (!g.freeblocks) continue;
        if (readblock(ext2, g.blockbitmap, bitmap)) {
            freememblock(bitmap);
            return -1;
        }
        for (int i = 0; i < ext2->sb.blockspergroup; i++) {
            if (!testbit(bitmap, i)) {
                block = ext2->sb.firstblock + gi * ext2->sb.blockspergroup + i;
                ext2->sb.numfreeblocks--;
                g.freeblocks--;
                setbit(bitmap, i);
                if (flushsb(ext2)) return -1;
                if (flushgroup(ext2, gi, &g)) return -1;
                if (writeblock(ext2, g.blockbitmap, bitmap)) return -1;
                goto end;
            }
        }
    }
end:
    freememblock(bitmap);
    return block;
}

static void freeblock(Ext2 *ext2, int block) {
    // TODO
}

static int power(int base, int exp) {
    if (exp == 0) return 1;
    return base * power(base, exp - 1);
}

static int getindirect(Ext2 *ext2, int block, int depth, int idx) {
    int ppb = ext2->blocksz / 4; // pointers per block
    uint32_t *tmp = allocmemblock(ext2);
    while (depth >= 0) {
        if (readblock(ext2, block, tmp)) {
            freememblock(tmp);
            return -1;
        }
        block = tmp[idx / power(ppb, depth)];
        idx %= power(ppb, depth);
        depth--;
    }
    freememblock(tmp);
    return block;
}

static int powersum(int base, int exp) {
    if (!exp) return 0;
    return power(base, exp) + powersum(base, exp - 1);
}

static int getinodeblock(Ext2 *ext2, Inode *inode, int idx) {
    uint64_t size = inodesize(inode);
    if (idx * ext2->blocksz >= size) return -1;
    if (idx < 12) return inode->blocks[idx];
    int ppb = ext2->blocksz / 4; // pointers per block
    if (idx < 12 + powersum(ppb, 1)) {
        return getindirect(ext2, inode->blocks[12], 0, idx - 12);
    }
    else if (idx < 12 + powersum(ppb, 2)) {
        return getindirect(ext2, inode->blocks[13], 1, idx - 12 - powersum(ppb, 1));
    }
    else if (idx < 12 + powersum(ppb, 3)) {
        return getindirect(ext2, inode->blocks[14], 2, idx - 12 - powersum(ppb, 2));
    }
    return -1;
}

static int ext2read(Vnode *vn, void *dst, int off, int count) {
    Ext2 *ext2 = vn->device;
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    if (!(inode.mode & EXT2_S_IFREG))
        return -1;
    uint64_t isz = inodesize(&inode);
    if (off >= isz) return 0;
    if (count <= 0) return 0;
    if (off + count > isz) count = isz - off;
    char *tmp = allocmemblock(ext2);
    int localblock = off / ext2->blocksz;
    int absblock = getinodeblock(ext2, &inode, localblock);
    if (absblock < 0 || readblock(ext2, absblock, tmp)) {
        freememblock(tmp);
        return -1;
    }
    int blockoff = off % ext2->blocksz;
    int blockrem = ext2->blocksz - blockoff;
    int len = blockrem < count ? blockrem : count;
    memcpy(dst, &tmp[blockoff], len);
    freememblock(tmp);
    return len;
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

static int allocinodeblock(Ext2 *ext2, Inode *inode, int relblock) {
    printf("allocating #%i\n", relblock);
    int block = allocblock(ext2);
    int ppb = ext2->blocksz / 4; // pointers per block
    if (block < 0)
        return -1;
    if (relblock < 12) {
        if (inode->blocks[relblock])
            return -1;
        inode->blocks[relblock] = block;
        return 0;
    }
    else if (relblock < 12 + powersum(ppb, 1)) {
        // TODO
    }
    freeblock(ext2, block);
    return -1;
}

static int truncate(Ext2 *ext2, uint32_t inum, int size) {
    Inode inode;
    if (readinode(ext2, &inode, inum))
        return -1;
    int oldsize = inodesize(&inode);
    printf("%i -> %i\n", oldsize, size);
    int numoldblocks = (oldsize + ext2->blocksz - 1) / ext2->blocksz;
    int numnewblocks = (size + ext2->blocksz - 1) / ext2->blocksz;
    // printf("numoldblocks %i\n", numoldblocks);
    // printf("numnewblocks %i\n", numnewblocks);
    while (numoldblocks < numnewblocks) {
        if (allocinodeblock(ext2, &inode, numoldblocks))
            return -1;
        numoldblocks++;
    }
    setinodesize(&inode, size);
    if (writeinode(ext2, inum, &inode))
        return -1;
    return 0;
}

static int ext2write(Vnode *vn, int off, int count, void *src) {
    Ext2 *ext2 = vn->device;

    // void *tmpbuf = allocmemblock(ext2);
    // for (int i = 0; i < 1; i++) {
    //     int block = allocblock(ext2);
    //     if (block < 0) return -1;
    //     if (readblock(ext2, block, tmpbuf)) return -1;
    //     memset(tmpbuf, 0, ext2->blocksz);
    //     if (writeblock(ext2, block, tmpbuf)) return -1;
    //     printf("allocated #%i\n", block);
    // }
    // freememblock(tmpbuf);
    // return count;

    int blockoff = off % ext2->blocksz;
    int blockrem = ext2->blocksz - blockoff;
    count = blockrem < count ? blockrem : count;
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return -1;
    if (!(inode.mode & EXT2_S_IFREG))
        return -1;
    if (off + count > inodesize(&inode)) {
        if (truncate(ext2, vn->vnum, off + count))
            return -1;
        if (readinode(ext2, &inode, vn->vnum))
            return -1;
    }
    int relblock = off / ext2->blocksz;
    int absblock = getinodeblock(ext2, &inode, relblock);
    if (absblock < 0) {
        printf("*** block #%i doesn't exist\n", relblock);
        return -1;
    }
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
    Ext2 *ext2 = parent->device;
    Inode inode;
    if (readinode(ext2, &inode, parent->vnum))
        return -1;
    if (!(inode.mode & EXT2_S_IFDIR))
        return -1;
    int i = 0;
    int offset = 0;
    uint8_t *tmp = allocmemblock(ext2);
    uint64_t isz = inodesize(&inode);
    while (offset < isz) {
        int block = getinodeblock(ext2, &inode, offset / ext2->blocksz);
        if (block < 0 || readblock(ext2, block, tmp)) {
            freememblock(tmp);
            return -1;
        }
        offset += ext2->blocksz;
        int boff = 0;
        while (boff < ext2->blocksz) {
            Ext2DirEnt *de = (void *)&tmp[boff];
            if (i == index) {
                memcpy(dst->name, de->name, de->namelen);
                dst->name[de->namelen] = 0;
                dst->vnum = de->inum;
                freememblock(tmp);
                return 0;
            }
            boff += de->reclen;
            i++;
        }
    }
    freememblock(tmp);
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

static int mkentry(Vnode *parent, char *name, uint32_t inum) {
    // Ext2 *ext2 = parent->device;
    // Inode inode;
    // if (readinode(ext2, &inode, parent->vnum))
    //     return -1;
    // if (!(inode.mode & EXT2_S_IFDIR))
    //     return -1;
    // int i = 0;
    // int offset = 0;
    // uint8_t *tmp = allocmemblock(ext2);
    // uint64_t isz = inodesize(&inode);
    // while (offset < isz) {
    //     int block = getinodeblock(ext2, &inode, offset / ext2->blocksz);
    //     if (block < 0 || readblock(ext2, block, tmp)) {
    //         freememblock(tmp);
    //         return -1;
    //     }
    //     offset += ext2->blocksz;
    //     int boff = 0;
    //     while (boff < ext2->blocksz) {
    //         Ext2DirEnt *de = (void *)&tmp[boff];
    //         if (i == index) {
    //             memcpy(dst->name, de->name, de->namelen);
    //             dst->name[de->namelen] = 0;
    //             dst->vnum = de->inum;
    //             freememblock(tmp);
    //             return 0;
    //         }
    //         boff += de->reclen;
    //         i++;
    //     }
    // }
    // freememblock(tmp);
    return -1;
}

static int ext2create(Vnode *parent, char *name) {
    printf("ext2 create\n");
    Ext2 *ext2 = parent->device;
    uint32_t inum = allocinode(ext2);
    if (!inum) return -1;
    printf("found free inode %i\n", inum);
    Inode inode;
    if (readinode(ext2, &inode, inum)) {
        freeinode(ext2, inum);
        return -1;
    }
    fillinode(&inode);
    if (writeinode(ext2, inum, &inode)) {
        freeinode(ext2, inum);
        return -1;
    }
    if (mkentry(parent, name, inum)) {
        freeinode(ext2, inum);
        return -1;
    }
    return 0;
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
    return 0;
}

static int blocktogroup(Ext2 *ext2, int block) {
    if (block < ext2->sb.firstblock)
        return -1;
    return (block - ext2->sb.firstblock) / ext2->sb.blockspergroup;
}

static int groupblock(Ext2 *ext2, int absblock) {
    return (absblock - ext2->sb.firstblock) % ext2->sb.blockspergroup;
}

static void checkused(Ext2 *ext2, int absblock) {
    int gi = blocktogroup(ext2, absblock);
    if (gi < 0) {
        printf("*** can't use block #%i\n", absblock);
        return;
    }
    Group g;
    if (readgroup(ext2, &g, gi)) {
        printf("*** couldn't read group\n");
        return;
    }
    uint8_t *bitmap = allocmemblock(ext2);
    if (readblock(ext2, g.blockbitmap, bitmap)) {
        printf("*** couldn't read block bitmap\n");
        freememblock(bitmap);
        return;
    }
    int relblock = groupblock(ext2, absblock);
    if (!testbit(bitmap, relblock)) {
        printf("*** #%i not marked used\n", absblock);
    }
    freememblock(bitmap);
}

static void check(Ext2 *ext2, Vnode *vn) {
    // printf("checking [%li][%s]\n", vn->vnum, vn->name);
    Inode inode;
    if (readinode(ext2, &inode, vn->vnum))
        return;
    int off = 0;
    while (off < inodesize(&inode)) {
        int relblock = off / ext2->blocksz;
        int absblock = getinodeblock(ext2, &inode, relblock);
        if (absblock < 0) {
            printf("*** bad block\n");
            return;
        }
        checkused(ext2, absblock);
        off += ext2->blocksz;
    }
    if (inode.mode & EXT2_S_IFDIR) {
        DirEnt de;
        int i = 2;
        while (ext2readdir(vn, &de, i++) == 0) {
            // printf("[%s]\n", de.name);
            Vnode e;
            fillvnode(ext2, &e, de.vnum);
            strcpy(e.name, de.name);
            check(ext2, &e);
        }
    }
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
    printf("checking...\n");
    check(ext2, dst);
    printf("done.\n");
    return 0;
}
