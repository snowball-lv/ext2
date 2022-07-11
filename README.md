WIP.  
Command line tool for manipulating ext2 images.  
An exercise in designing a VFS layer.

## Usage

```bash
    ./bin/ext2 disk.img cmd [operand...]
```

### Commands supported

- `ls path` - list directory content
- `cat path` - print file content
- `stat path` - print information about file or directory
- `write path` - overwrite file with `stdin`
- `create path` - create file
- `mkdir path` - create directory
- `unlink path` - delete file or directory
- `symlink target linkpath` - create symlink `linkpath` that points to `target`
- `link oldpath newpath` - create hard link `newpath` referencing inode of `oldpath`

## Build

```bash
    make
```

## Resources

- https://wiki.osdev.org/Ext2
- https://www.nongnu.org/ext2-doc/ext2.html
