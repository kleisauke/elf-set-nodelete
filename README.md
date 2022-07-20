# elf-set-nodelete

Tweaked version of [termux-elf-cleaner](https://github.com/termux/termux-elf-cleaner)
to set `RTLD_NODELETE` on ELF files.

## Usage

```bash
$ meson setup build --prefix=/usr --buildtype=release
$ meson compile -C build
$ meson install -C build

# Inspect
$ readelf -Wd /usr/lib64/libvips.so.42 | grep -F FLAGS_1
 0x000000006ffffffb (FLAGS_1)            Flags: NOW

# Dry run
$ sudo elf-set-nodelete --dry-run /usr/lib64/libvips.so.42
elf-set-nodelete: Replacing DF_1_* flags 1 with 9 in '/usr/lib64/libvips.so.42'

# For real
$ sudo elf-set-nodelete /usr/lib64/libvips.so.42
elf-set-nodelete: Replacing DF_1_* flags 1 with 9 in '/usr/lib64/libvips.so.42'

# Inspect
$ readelf -Wd /usr/lib64/libvips.so.42 | grep -F FLAGS_1
 0x000000006ffffffb (FLAGS_1)            Flags: NOW NODELETE
```
