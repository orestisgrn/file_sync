# FileSync System

Multi process linux program for real-time file synchronization, written in C. Created during Systems Programming course in University of Athens, Greece.

## Building

Code is compiled with: ``` $ make ```

Object files and executables are cleared with: ``` $ make clean ```

## Executing and Usage

Program consists of 3 executables:

- __fss_manager:__
\
\
Usage: ``` ./fss_manager -l <manager_logfile> -c <config_file> -n <worker_limit> ```
\
\
This is the main process of the program. It's the process that monitors given directories and initiates their synchronization with another directory. The directories to be synchronized are either given by the console process (explained below), or given in the config file as tuples of paths (either absolute or relative), which are separated by white characters, and the first path is the source directory to be monitored, and the second path is the target directory where the contents of the source will be copied to (see example config file in examples dir). Each source maps to one target. At execution, the directories in config are synchronized, meaning the (regular) files of source are copied in its target. After that, all changes in the monitored source directories are copied in their mapped target directories. The arguments given for the execution are a logfile to record process logs and the maximum number of worker processes, which copy the source files, that can be running at any moment.
\
\
__Only regular files in the source directories are synchronized (for now). Thus, directories inside the source directories aren't synchronized recursively.__

- __fss_console:__
\
\
Usage: ``` ./fss_console -l <console-logfile> ```
\
\
This process implements a console in which commands can be communicated to the manager process to be executed. The commands can be:
  - __add \<source\> \<target\>:__ Adds \<source\>/\<target\> directories to be monitored after fully synchronizing them. 
  - __cancel \<source\>:__ Ceases the monitoring of \<source\> directory temporarily.
  - __status \<source\>:__ Returns info about the monitored directory, like this:
    ```
    [2025-04-10 10:00:01] Status requested for /home/user/docs
    Directory: /home/user/docs
    Target: /backup/docs
    Last Sync: 2025-03-30 14:25:30
    Errors: 0
    Status: Active
    ```
  - __sync \<source\>:__ Synchronizes all files of \<source\> directory on demand if none of them are being synchronized, and restarts the monitoring of \<source\> if it was ceased by the cancel command.
  - __shutdown__: Shuts down the manager process and the console itself, after waiting for any pending synchronization tasks to be completed.

- __worker:__
\
\
Process executed after forking by fss_manager. Not to be executed on demand.

## More documentation and implementation details

More details about the program and its implementation can be found in the docs directory, where the original assignment description along with my implementation's details can be found.
