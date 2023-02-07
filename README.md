# cshred - Overwrite a file to hide its contents

## Description

`cshred` is a minimalistic clone of the popular UNIX utility `shred`.
It's meant to be a drop-in replacement for it.

## Usage

`cshred` works on one or more input files, it overwrites the file with random
bytes multiple times and optionally deletes the file.
Example `cshred -u ./file.txt` first overwrites the contents of the file and
then removes the file.

## Options

The supported options and their meaning is the same as regular shred.
See `man 1 shred` for more info.

## Installation
```
git clone https://github.com/begone-prop/cshred.git
cd cshred
gcc ./cshred.c -o ./cshred
```
