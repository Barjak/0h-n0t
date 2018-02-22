# 0h n0t

This is an incomplete clone of [Martin Kool's](https://github.com/mrtnkl) game [0h n0](http://0hn0.com/).

A couple years ago, I wanted to play the game on larger boards, but the original board generation algorithm, while perfectly fine for small boards, scaled poorly. I tried reimplementing the original algorithm in C, but it was still slow, so I scrapped the project.

A couple years later I built a toy constraint solver and decided to modify it to create large 0h n0 boards. It's now a playable desktop game, albeit with a very hacky GUI that I wrote in about 2 hours and that doesn't really support animation.

Most sane people will prefer Martin's original implementation, which is available on multiple platforms and has animation.

### Building on Windows 10 (x86-64):

Clone the repository and use the CMake GUI to generate the build, then build the target `win64`. Only tested with VS2017.

Open the `0hn0t-win64` directory and run `0hn0t.exe`.

### Building on Debian variants:

`sudo apt-install love`

`git clone https://github.com/Barjak/0h-n0t.git && cd 0hn0t`

`cmake -DCMAKE_BUILD_TYPE=Release . && make unix && ./0hn0t.sh`

### Building on other Linux and OSX:

Untested, but you can try `cmake -DCMAKE_BUILD_TYPE=Release . && make unix`.
