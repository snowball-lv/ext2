#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ext2/vfs.h>
#include <ext2/fdev.h>
#include <ext2/ext2.h>

static void usage() {
    printf("Usage:\n%4sext2 image command\n", "");
}

static void ls(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** ls requires path\n");
        exit(1);
    }
    char *path = argv[0];
    Vnode dir;
    if (vfsresolve(root, root, &dir, path)) {
        printf("*** no such file [%s]\n", path);
        exit(1);
    }
    if ((dir.flags & VFS_DIR) != VFS_DIR) {
        printf("*** not a dir [%s]\n", path);
        exit(1);
    }
    DirEnt de;
    int i = 0;
    int rv;
    while ((rv = vfsreaddir(&dir, &de, i)) == 0) {
        printf("%2i: %3li %s\n", i, de.vnum, de.name);
        i++;
    }
}

static void cat(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** cat requires path\n");
        exit(1);
    }
    char *path = argv[0];
    Vnode file;
    if (vfsresolve(root, root, &file, path)) {
        printf("*** no such file [%s]\n", path);
        exit(1);
    }
    char buf[4096];
    int off = 0;
    int n;
    while ((n = vfsread(&file, buf, off, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, stdout);
        off += n;
    }
}

static void create(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** create requires path\n");
        exit(1);
    }
    char *path = argv[0];
    if (vfscreate(root, path, 0)) {
        printf("*** couldn't create [%s]\n", path);
        exit(1);
    }
}

static void write(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** write requires path\n");
        exit(1);
    }
    char *path = argv[0];
    Vnode file;
    if (vfsresolve(root, root, &file, path)) {
        printf("*** no such file [%s]\n", path);
        exit(1);
    }
    if (vfstruncate(&file)) {
        printf("*** couldn't truncate [%s]\n", path);
        exit(1);
    }
    char buf[256];
    int off = 0;
    int r;
    while ((r = fread(buf, 1, sizeof(buf), stdin))) {
        int w = vfswrite(&file, off, r, buf);
        if (w != r) {
            printf("*** %i of %i written\n", w, r);
            exit(1);
        }
        off += w;
    }
}

static void unlink(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** unlink requires path\n");
        exit(1);
    }
    char *path = argv[0];
    if (vfsunlink(root, path)) {
        printf("*** couldn't unlink [%s]\n", path);
        exit(1);
    }
}

static void mkdir(Vnode *root, int argc, char **argv) {
    if (!argc) {
        printf("*** mkdir requires path\n");
        exit(1);
    }
    char *path = argv[0];
    if (vfscreate(root, path, 1)) {
        printf("*** couldn't create [%s]\n", path);
        exit(1);
    }
}

static void symlink(Vnode *root, int argc, char **argv) {
    if (argc < 2) {
        printf("*** symlink requires path and value\n");
        exit(1);
    }
    char *value = argv[0];
    char *path = argv[1];
    if (vfssymlink(root, path, value)) {
        printf("*** couldn't create [%s]\n", path);
        exit(1);
    }
}

static int parsepath(char *dir, char *name, char *path) {
    char *slash = strrchr(path, '/');
    if (!slash) {
        strcpy(dir, "/");
        strcpy(name, path);
        return 0;
    }
    int dirlen = slash - path;
    strncpy(dir, path, dirlen);
    dir[dirlen] = 0;
    slash += 1;
    int namelen = path + strlen(path) - slash;
    strncpy(name, slash , namelen);
    name[namelen] = 0;
    return 0;
}

static void link(Vnode *root, int argc, char **argv) {
    if (argc < 2) {
        printf("*** link requires old and new path\n");
        exit(1);
    }
    char *oldpath = argv[0];
    char *newpath = argv[1];
    Vnode oldvn;
    Vnode newvn;
    char newdir[MAX_PATH];
    char newname[MAX_NAME];
    if (vfsresolve(root, root, &oldvn, oldpath)) {
        printf("*** couldn't resolve [%s]\n", oldpath);
        exit(1);
    }
    if (parsepath(newdir, newname, newpath)) {
        printf("*** couldn't parse new path\n");
        exit(1);
    }
    if (vfsresolve(root, root, &newvn, newdir)) {
        printf("*** couldn't resolve [%s]\n", newdir);
        exit(1);
    }
    if (vfslink(&oldvn, &newvn, newname)) {
        printf("*** couldn't create hard link [%s]\n", newpath);
        exit(1);
    }
}

typedef struct {
    char *name;
    void (*func)(Vnode *root, int argc, char **argv);
} Cmd;

static Cmd CMDTAB[] = {
    {"ls", ls},
    {"cat", cat},
    {"create", create},
    {"write", write},
    {"unlink", unlink},
    {"mkdir", mkdir},
    {"symlink", symlink},
    {"link", link},
    {0},
};

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        exit(1);
    }
    char *img = argv[1];
    char *cmd = argv[2];
    Vnode bdev;
    if (mkfdev(&bdev, img)) {
        printf("*** couldn't open [%s]\n", img);
        exit(1);
    }
    Vnode ext2;
    if (mkext2(&ext2, &bdev)) {
        printf("*** couldn't init ext2\n");
        exit(1);
    }
    for (Cmd *cp = CMDTAB; cp->name; cp++) {
        if (strcmp(cp->name, cmd) == 0) {
            cp->func(&ext2, argc - 3, argv + 3);
            return 0;
        }
    }
    printf("*** no such command [%s]\n", cmd);
    exit(1);
}
