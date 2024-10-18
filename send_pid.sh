#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <pid>"
    exit 1
fi

pid=$1

pipe="/tmp/user_space_page_migration_fifo"

if [ ! -p "$pipe" ]; then
    echo "Error: Pipe $pipe does not exist."
    exit 1
fi

echo "$pid" > "$pipe"