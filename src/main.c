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
    if (vfsresolve(root, &dir, path)) {
        printf("*** no such file [%s]\n", path);
        exit(1);
    }
    DirEnt de;
    int i = 0;
    while (vfsreaddir(&dir, &de, i) == 0) {
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
    if (vfsresolve(root, &file, path)) {
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
        printf("*** cat requires path\n");
        exit(1);
    }
    char *path = argv[0];
    if (vfscreate(root, path)) {
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
    if (vfsresolve(root, &file, path)) {
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
