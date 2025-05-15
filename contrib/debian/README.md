
Debian
====================
This directory contains files used to package c_noted/c_note-qt
for Debian-based Linux systems. If you compile c_noted/c_note-qt yourself, there are some useful files here.

## c_note: URI support ##


c_note-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install c_note-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your c_note-qt binary to `/usr/bin`
and the `../../share/pixmaps/c_note128.png` to `/usr/share/pixmaps`

c_note-qt.protocol (KDE)

