Authors:        Dan DeGenaro, Tian Li (Net ID: drd92, tl995)
Date:           Feb 27, 2026
Last Updated:   Apr 28, 2026
Purpose:        Bash emulator project.
Platform:       Linux, Solaris, BSD
gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
Version:        4.0



File structure:          
                bash_emulator/
                    Documents/
                        README.txt (THIS FILE)
                    include/
                        shell.h
                    src/
                        main.c
                        shell_job_control.c
                        shell_pipe.c
                        shell.c
                    .gitignore
                    Makefile



Installation and set-up:

    Ensure a directory structure as specified above. You may run:

        - `make all` to build object files if they don't already exist.
            You will need to do this at first installation. Aliased to
            `make`.

        - `make clean` to remove any current object files. Also removes draft
            files (ending in ~).

        - `make rebuild` to execute the `make clean` and `make all` behaviors.

        - `make run` to run the executable.



Expected behavior:

    Works the same as bash, except missing some signals such as arrow key navigation and tab to
        autocomplete.
    Some print messages may look a bit different as well, notably from the `fg`, `bg`, and `jobs`
        commands. The prompt is also more simplistic, and the hello/goodbye messages are as well.
    
    Implements from scratch:
        - `exit`: Exits the program.
        - `cd`: Changes directory.
        - `pwd`: Displays the current directory path.
        - `jobs`: Displays all jobs running in the background.
        - `fg`: Moves a job to the foreground (integer ID or %n for n-th recent job).
        - `bg`: Works like `fg` but moves jobs to the background.
        - Ctrl+C to terminate the foreground job.
        - Ctrl+Z to stop the foreground job.
    
    Implements any other program on the execution path as a child process.
