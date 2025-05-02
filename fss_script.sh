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
        path_name=$arg
        next_is_path=F
    elif [ "$next_is_command" = "T" ]; then
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

declare -A sync_info_arr

if [ "$command_name" = "listAll" ]; then
    if [ -f "$path_name" ]; then
        while IFS= read -r line; do # match five brackets
            echo "$line" | grep -P "^\[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\]$" > /dev/null
            if [ $? -eq 0 ]; then
                source=$(echo "$line" | awk -F'[\\[\\]]' '{print $4}')  # get second argument (source_dir)
                IFS= read -r success_code
                sync_info_arr["$source"]="${line} ${success_code}"
            fi
        done < "$path_name"
    else
        echo "Invalid file: $path_name"
        exit 1
    fi
    for record in "${sync_info_arr[@]}"; do
        echo "$record" | awk -F'[\\[\\]]' '{print $4, "->", $6, "[Last Sync:", $2"]", "["$12"]"}'
    done
elif [ "$command_name" = "listMonitored" ]; then
    declare -A stopped
    if [ -f "$path_name" ]; then
        while IFS= read -r line; do
            echo "$line" | grep -P "^\[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\]$" > /dev/null
            if [ $? -eq 0 ]; then
                source=$(echo "$line" | awk -F'[\\[\\]]' '{print $4}')
                IFS= read -r success_code
                sync_info_arr["$source"]="${line} ${success_code}"
            else 
                read -r -a words <<< "$line"
                echo "$line" | grep "Syncing directory" > /dev/null
                if [ $? -eq 0 ]; then
                    unset stopped["${words[4]}"]        # See format of syncing dir
                else
                    echo "$line" | grep "Monitoring stopped" > /dev/null
                    if [ $? -eq 0 ]; then
                        stopped["${words[5]}"]=1
                    fi
                fi
            fi
        done < "$path_name"
    else
        echo "Invalid file: $path_name"
        exit 1
    fi
    for key in "${!sync_info_arr[@]}"; do
        if [[ ! -v stopped["$key"] ]]; then
            echo "${sync_info_arr["$key"]}" | awk -F'[\\[\\]]' '{print $4, "->", $6, "[Last Sync:", $2"]"}'
        fi
    done
elif [ "$command_name" = "listStopped" ]; then      # same as listMonitored, but prints only stopped
    declare -A stopped
    if [ -f "$path_name" ]; then
        while IFS= read -r line; do
            echo "$line" | grep -P "^\[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\] \[[^\]]+\]$" > /dev/null
            if [ $? -eq 0 ]; then
                source=$(echo "$line" | awk -F'[\\[\\]]' '{print $4}')
                IFS= read -r success_code
                sync_info_arr["$source"]="${line} ${success_code}"
            else 
                read -r -a words <<< "$line"
                echo "$line" | grep "Syncing directory" > /dev/null
                if [ $? -eq 0 ]; then
                    unset stopped["${words[4]}"]        # See format of syncing dir
                else
                    echo "$line" | grep "Monitoring stopped" > /dev/null
                    if [ $? -eq 0 ]; then
                        stopped["${words[5]}"]=1
                    fi
                fi
            fi
        done < "$path_name"
    else
        echo "Invalid file: $path_name"
        exit 1
    fi
    for key in "${!sync_info_arr[@]}"; do
        if [[ -v stopped["$key"] ]]; then
            echo "${sync_info_arr["$key"]}" | awk -F'[\\[\\]]' '{print $4, "->", $6, "[Last Sync:", $2"]"}'
        fi
    done
elif [ "$command_name" = "purge" ]; then
    if [ -f "$path_name" ]; then
        echo "Deleting $path_name..."
        rm "$path_name"
    elif [ -d "$path_name" ]; then
        echo "Deleting $path_name..."
        rm -r "$path_name"
    else
        echo "Invalid path: $path_name"
        exit 1
    fi
    echo Purge complete.
else
    echo "Invalid command: $command_name"
fi
