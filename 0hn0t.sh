#!/bin/bash

cd $(dirname $(readlink -f $0))/0hn0t-unix
if [ $? -eq 0 ]; then
        love 0hn0t.love
else
        echo "Game is not yet built."
fi
