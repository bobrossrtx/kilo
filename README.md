# Kilo
## A nano like a terminal text editor

### Setup
#### Building from source
To build a kilo from the source, you can either build through a unix type operating system, or you can build kilo through WSL, Cygwin, or even Windows bash if you are running on Microsoft Windows 10.

First off, make is a necessery tool to know when working and building C/C++ code, it is so usefull and is what will be used in this application for building. So if you want, you can install make to build the binaries for the project. With make, you can set up functions that execute commands that are made to make building C/C++ Projects easier.
To install make on Debian-based Linux distributions, you can run ```sudo apt install make```.

You should be able to compile straight from there, but just in case, you might need to install GNU/gcc, which is a C Runtime compile that allows the user to compiler C into binary executables. To install, it is as simple as typing ```sudo apt install gcc``` into your terminal.

An alternative to make is just building from the command line, if you needed to install gcc, you will be able to install straight from the command line. I recommend that if there is already a kilo executable, just to remove that binary so it does not get in the way of the build process. but, with the gcc command, you can run ```cc src/kilo.c -o kilo -Wall -Wextra -pedantic -std=c99``` and that will compile straight into binaries that can be used

From then on, you can easily install Kilo. if you are using make, you can build the application by executing the command ```make all``` into your terminal and it will remove any already existing binaries, and replace them with the updated ones.

### Use
... Under Construction ...
