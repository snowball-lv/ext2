Command line tool and library for managing ext2 images.
Exercise for designing filesystem drivers to plug into a VFS layer.

## Usage

```bash
    ./bin/ext2 disk.img cmd path
```

### Commands supported

- `ls` - list directory content
- `cat` - print file content
- `touch` - create file
- `mkdir` - create directory
- `write` - overwrite file with `stdin`
- `rm` - delete file
- `rmdir` - dlete directory

## Build

```bash
    make
```

## Resources

- https://wiki.osdev.org/Ext2
- https://www.nongnu.org/ext2-doc/ext2.html