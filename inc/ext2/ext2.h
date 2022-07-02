#pragma once

typedef struct {
    uint32_t numinodes;
    uint32_t numblocks;
    uint32_t numreservedblocks;
    uint32_t numfreeblocks;
    uint32_t numfreeinodes;
    uint32_t firstblock;
    uint32_t blockszshift;
    uint32_t fragszshift;
    uint32_t blockspergroup;
    uint32_t fragspergroup;
    uint32_t inodespergroup;
    uint32_t mounttime;
    uint32_t writetime;
    uint16_t nummounts;
    uint16_t maxmounts;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t revminor;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creatorid;
    uint32_t revmajor;
    uint16_t defuid;
    uint16_t defgid;
    // extended fields
    uint32_t firstinode;
    uint16_t inodesz;
    uint16_t blockgroup;
    uint32_t featuresopt;
    uint16_t featuresreq;
    uint16_t featuresro;
    char uuid[16];
    char name[16];
    char lastmount[64];
    uint32_t compression;
    uint8_t preallocfile;
    uint8_t preallocdir;
    uint16_t _alignment;
    char jrnluuid[16];
    uint32_t jrnlinode;
    uint32_t jrnldev;
    uint32_t orphan;
} Superblock;

typedef struct {
    uint32_t blockbitmap;
    uint32_t inodebitmap;
    uint32_t indoetab;
    uint16_t freeblocks;
    uint16_t freeinodes;
    uint16_t numdirs;
    uint16_t _padding;
    char _reserved[12];
} Group;

typedef struct {
    Vnode *bdev;
    Superblock sb;
    uint32_t inodesz;
    uint32_t blocksz;
    uint32_t numgroups;
    uint32_t ppb; // pointers per block
} Ext2;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t numlinks;
    uint32_t sectors;
    uint32_t flags;
    uint32_t osval1;
    uint32_t blocks[15];
    uint32_t generation;
    uint32_t fileacl;
    uint32_t diracl;
    uint32_t faddr;
    char osval2[12];
} Inode;

typedef uint32_t Inum;

typedef struct {
    Inum inum;
    uint16_t reclen;
    uint8_t namelen;
    uint8_t filetype;
    char name[];
} Ext2DirEnt;

int mkext2(Vnode *dst, Vnode *bdev);
