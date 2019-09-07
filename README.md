# UEFI Junk toolz

* My junk applications for UEFI

## Requirements

* rake (ruby)
* llvm (clang/lld)
* qemu

## Cross Compiling

### x64 (amd64)

```
$ rake ARCH=x64
$ rake run ARCH=x64
```

### i386

```
$ rake ARCH=i386
$ rake run ARCH=i386
```

### aarch64 (arm64)

```
$ rake ARCH=aa64
$ rake run ARCH=aa64
```

### aarch32 (arm32)

* Currently not supported :cry:

## License

* MIT

## References

* [liumOS](https://github.com/hikalium/liumos)
* [Unofficial EDK2 nightly build](https://retrage.github.io/edk2-nightly/)
