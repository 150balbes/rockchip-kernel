#!/bin/bash

find ./ -type f | while read file
do
  if file "$file" | grep -q "x86-64"
  then
    echo "$file"
    rm "$file"
  fi
done
