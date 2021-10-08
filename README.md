# swappeds

swappeds is a tool that gives an estimate of swapped memory per
process.

Only Linux is currently supported.

## Quick start
Download swappeds and build it:

    git clone https://github.com/SSYSS000/swappeds.git
    cd swappeds
    make

Run it:

    ./swappeds

Options:
        
    -c    produce an estimated grand total
    -h    show help
    -n    show the name of the processes

## Install instructions
Run `make install` and `make uninstall` to install and uninstall swappeds.
The default install path prefix is /usr/local/. If you want,
change the prefix by passing `PREFIX=/path/to/dir` to make.

## Contribution
If you discover bugs, making them known is appreciated.
