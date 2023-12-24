# zExplorer
A terminal-based file explorer.

This program is made with the objective of providing a lightweight file explorer, accessible via terminal interface and more user-friendly than the traditional commands.

---
##Dependencies

A dependency for this program is [ncurses](https://invisible-island.net/ncurses/announce.html)

---

##Requirements for compiling

1. g++
2. ncurses headers and binaries
3. source code

###Command
####Linux platforms

    g++ -std=c++17 -o bin/zExplorer src/*.cpp -lncurses -Wl,-rpath=.

####Windows platforms

    g++ -std=c++17 -I<path to ncurses headers> -o bin/zExplorer src/*.cpp -lncurses -DNCURSES_STATIC

Any bugfixes and pull requests, especially on managing errors, are appreciated.