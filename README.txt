
========= winrenice - change the priority class of existing processes ==========

----- 1. Introduction -----

This command line utility can change the priority class of existing processes.
Whereas tools such as start.exe can launch processes with a certain priority,
only graphical tools such as taskmgr.exe exist that can change the priority of
allready existing processes.
winrenice can be called from the command line or from scripts and can therefor
be used to automatate the rescheduling of the currently running processes of a
system.

The project is hosted on GitHub, a "social coding" website that hosts public git
repositories for free and provides various collaboration services such as 
collaborator management, issue tracking, wikis, downloads, code review and
others.
The GitHub project page is here: https://github.com/fat-lobyte/winrenice


----- 2. Download and installation -----

This tool can be downloaded from the GitHub download page:
https://github.com/fat-lobyte/winrenice/downloads

To ensure that this utility works on your system, you have to choose the correct
CPU architecture from the available downloads.
If you have a 32-Bit operating system, choose the "x86" download; if you have
a 64-Bit operating system, choose the "x64" download.

To install this utility, simply unpack the archive to a known location. Then
you either have to prefix the path to this location everytime you invoke this
utility, or add this location to the system (or user) PATH environment variable.

----- 3. Compiling from source code -----

Currently, this utility can only be compiled with MinGW (or MinGW-w64) that has
GCC >= 4.7.

As soon as a freely available version of Visual Studio that supports
enough of the recent C++ standard (C++11) is available, it will be ported to
Visual Studio.


--- 3.1 Compilation with MinGW (or MinGW-64) ---

1. Change to the directory of the source code
2a. Invoke mingw32-make.exe with the provided Makefile:
    mingw32-make.exe -f makefile.gcc

This will build winrenice.exe linked to any MinGW libraries dynamically. If you
want to link MinGW libraries statically, add "-static" to the CXXFLAGS
environment variable:

2b.
    set CXXFLAGS=-static
    mingw32-make.exe -f makefile.gcc

The resulting file winrenice.exe can be directly be used.


----- 4. Changes to the utility and feedback -----

This utility is released under a BSD-style license, which means that it is
open source.

Any changes to the source code can be made. If you decide to change this
utility, please consider to contribute your changes back to the original author.
This is not required by the licensed, but very appreciated.

Any feedback, suggestions, bug reports or wishes are welcome. If you want to
express any of those, it's best if you sign up on GitHub and send me a message
or a pull request.
If you don't want to do that, or you have something to tell me that you think
isn't suited for public, mail me to this address: fat.lobyte9@gmail.com


----- 5. Usage -----

WINRENICE [/H | /?] [/V] [/I | /D] <priority> [/E] <target>

Description:
    This program changes the priority class for existing processes.
    Processes can be specified with a numerical Process ID or by the executable
    filename.
    The priority can increased, decreased or set to a specific value.

Parameter list:
    <priority>      If the /I or /D flags are ommited, this the target 
                    process(es) will be set to this priority class. The priority
                    class can be one of:
                        IDLE
                        BELOWNORMAL
                        NORMAL
                        ABOVENORMAL
                        HIGH
                        REALTIME
                        
                    If the either the /I or /D flag is present, <priority> is
                    interpreted as an integer and the priority class of the 
                    target process(es) will be increased or decreased by
                    <priority> steps.
                        
    <target>        Denotes the target process or processes.
                    With the /E flag absent, <target> is the numerical 
                    process ID of the target process.
                    With the /E flag present, <target> is the basename of an
                    executable file. Then, the priority of all processes that
                    were started by this executable file will be changed.
    
    /E              Interpret <target> as the basename of an executable instead
                    of a process ID
    
    /I              Increase priority by <priority> steps
    
    /D              Decrease priority by <priority> steps

    /H or /?        Display program help
    
    /V              Display program version
    
Notes:
    For certain processes (such as system processes), this utility needs
    elevated access privileges or the operation will fail.
    
    The 32-bit version of this utility cannot be used on 64-bit processes.
    Please download the appropriate version for your operating system from
    https://github.com/fat-lobyte/winrenice/downloads .

Examples:
    Set priority of process 2044 to IDLE:
        winrenice IDLE 2044
        
    Set priority of all processes started by "cmd.exe" to HIGH:
        winrenice HIGH /E cmd.exe
    
    Decrease priority of all processes that were started by svchost.exe by 2:
        winrenice /D 2 /E svchost.exe

Copyright (c) 2012, Alexander Korsunsky <fat.lobyte9@gmail.com>
