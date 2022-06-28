#include <stdint.h>
#include <stdio.h>
#include <ext2/vfs.h>
#include <ext2/ext2.h>
#include <ext2/debug.h>

#define EXT2_REV_1 1

static void uuidtostr(char *dst, char *uuid) {
    for (int i = 0; i < 4; i++)
        dst += sprintf(dst, "%.2x", uuid[i]);
    dst += sprintf(dst, "-%.2x%.2x", uuid[4 + 0], uuid[4 + 1]);
    dst += sprintf(dst, "-%.2x%.2x", uuid[6 + 0], uuid[6 + 1]);
    dst += sprintf(dst, "-%.2x%.2x-", uuid[8 + 0], uuid[8 + 1]);
    for (int i = 0; i < 6; i++)
        dst += sprintf(dst, "%.2x", uuid[10 + i]);
}

void dumpsb(Ext2 *ext2) {
    Superblock *sb = &ext2->sb;
    char uuidbuf[64];
    printf("Superblock:\n");
    printf("%4snuminodes %u\n", "", sb->numinodes);
    printf("%4snumblocks %u\n", "", sb->numblocks);
    printf("%4snumreservedblocks %u\n", "", sb->numreservedblocks);
    printf("%4snumfreeblocks %u\n", "", sb->numfreeblocks);
    printf("%4snumfreeinodes %u\n", "", sb->numfreeinodes);
    printf("%4sfirstblock %u\n", "", sb->firstblock);
    printf("%4sblockszshift %u (%i)\n", "", sb->blockszshift, ext2->blocksz);
    printf("%4sfragszshift %u\n", "", sb->fragszshift);
    printf("%4sblockspergroup %u\n", "", sb->blockspergroup);
    printf("%4sfragspergroup %u\n", "", sb->fragspergroup);
    printf("%4sinodespergroup %u\n", "", sb->inodespergroup);
    printf("%4smounttime %u\n", "", sb->mounttime);
    printf("%4swritetime %u\n", "", sb->writetime);
    printf("%4snummounts %u\n", "", sb->nummounts);
    printf("%4smaxmounts %u\n", "", sb->maxmounts);
    printf("%4smagic %#x\n", "", sb->magic);
    printf("%4sstate %u\n", "", sb->state);
    printf("%4serrors %u\n", "", sb->errors);
    printf("%4srevminor %u\n", "", sb->revminor);
    printf("%4slastcheck %u\n", "", sb->lastcheck);
    printf("%4scheckinterval %u\n", "", sb->checkinterval);
    printf("%4screatorid %u\n", "", sb->creatorid);
    printf("%4srevmajor %u\n", "", sb->revmajor);
    printf("%4sdefuid %u\n", "", sb->defuid);
    printf("%4sdefgid %u\n", "", sb->defgid);
    if (sb->revmajor < EXT2_REV_1)
        return;
    printf("Extended fields:\n");
    printf("%4sfirstinode %u\n", "", sb->firstinode);
    printf("%4sinodesz %u\n", "", sb->inodesz);
    printf("%4sblockgroup %u\n", "", sb->blockgroup);
    printf("%4sfeaturesopt %u\n", "", sb->featuresopt);
    printf("%4sfeaturesreq %u\n", "", sb->featuresreq);
    printf("%4sfeaturesro %u\n", "", sb->featuresro);
    uuidtostr(uuidbuf, sb->uuid);
    printf("%4suuid %s\n", "", uuidbuf);
    printf("%4sname [%s]\n", "", sb->name);
    printf("%4slastmount [%s]\n", "", sb->lastmount);
    printf("%4scompression %u\n", "", sb->compression);
    printf("%4spreallocfile %u\n", "", sb->preallocfile);
    printf("%4spreallocdir %u\n", "", sb->preallocdir);
    // printf("%4s_alignment %u\n", "", sb->_alignment);
    uuidtostr(uuidbuf, sb->uuid);
    printf("%4sjrnluuid %s\n", "", uuidbuf);
    printf("%4sjrnlinode %u\n", "", sb->jrnlinode);
    printf("%4sjrnldev %u\n", "", sb->jrnldev);
    printf("%4sorphan %u\n", "", sb->orphan);
    printf("Computed:\n");
    printf("%4sinodesz %i\n", "", ext2->inodesz);
    printf("%4snumgroups %i\n", "", ext2->numgroups);
    printf("%4sblocksz %i\n", "", ext2->blocksz);
}

void dumpgroup(Group *g) {
    printf("%4sblockbitmap %u\n", "", g->blockbitmap);
    printf("%4sinodebitmap %u\n", "", g->inodebitmap);
    printf("%4sindoetab %u\n", "", g->indoetab);
    printf("%4sfreeblocks %u\n", "", g->freeblocks);
    printf("%4sfreeinodes %u\n", "", g->freeinodes);
    printf("%4snumdirs %u\n", "", g->numdirs);
}

void dumpinode(Inode *inode) {
    printf("Inode\n");
    printf("%4smode %u\n", "", inode->mode);
    printf("%4suid %u\n", "", inode->uid);
    printf("%4ssize %u\n", "", inode->size);
    printf("%4satime %u\n", "", inode->atime);
    printf("%4sctime %u\n", "", inode->ctime);
    printf("%4smtime %u\n", "", inode->mtime);
    printf("%4sdtime %u\n", "", inode->dtime);
    printf("%4sgid %u\n", "", inode->gid);
    printf("%4snumlinks %u\n", "", inode->numlinks);
    printf("%4ssectors %u\n", "", inode->sectors);
    printf("%4sflags %u\n", "", inode->flags);
    printf("%4sosval1 %u\n", "", inode->osval1);
    // printf("%4sblocks %u\n", "", inode->blocks);
    printf("%4sgeneration %u\n", "", inode->generation);
    printf("%4sfileacl %u\n", "", inode->fileacl);
    printf("%4sdiracl %u\n", "", inode->diracl);
    printf("%4sfaddr %u\n", "", inode->faddr);
    // printf("%4sosval2 %u\n", "", inode->osval2);
}
