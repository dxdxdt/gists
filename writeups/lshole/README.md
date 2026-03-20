# lshole
List holes in files in the "POSIX way".

This is a general implementation that finds and lists holes in each file using
`SEEK_DATA` and `SEEK_HOLE`. A platform that provides the interface usually
provides a userland util that can be used to do what this program(`lshole`) can
do. For example, Linux users can enjoy `fallocate -r ...`. This program serves
as an alternative or example util for other platforms where such userland util
is not provided.
