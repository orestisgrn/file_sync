#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 -p <path> -c <command>"
    exit 1
fi

path_given=F
next_is_path=F
command_given=F
next_is_command=F

for arg in "$@"; do
    if [ "$arg" = "-p" ]; then
        if [ "$path_given" = "T" ]; then
            echo "-p argument given twice"
            exit 1
        fi
        path_given=T
        next_is_path=T
    elif [ "$arg" = "-c" ]; then
        if [ "$command_given" = "T" ]; then
            echo "-c argument given twice"
            exit 1
        fi
        command_given=T
        next_is_command=T
    elif [ "$next_is_path" = "T" ]; then
        echo "Path: $arg"
        path_name=$arg
        next_is_path=F
    elif [ "$next_is_command" = "T" ]; then
        echo "Command: $arg"
        command_name=$arg
        next_is_command=F
    fi   
done

if [ "$path_given" = "F" ] || [ "$next_is_path" = "T" ]; then
    echo "Path not given"
    exit 1
fi

if [ "$command_given" = "F" ] || [ "$next_is_command" = "T" ]; then
    echo "Command not given"
    exit 1
fi

if [ -f "$path_name" ]; then
    while IFS= read -r line; do # match five brackets
        echo "$line" | grep -P "^\[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\]$" > /dev/null
        if [ $? -eq 0 ]; then
            echo "$line" | awk -F'[\\[\\]]' '{for(i=2;i<=NF;i+=2) print $i}'
            IFS= read -r success_code
            echo "$success_code" | awk -F'[\\[\\]]' '{print $2}'
        fi
    done < "$path_name"
else
    echo "Invalid file: $path_name"
fi