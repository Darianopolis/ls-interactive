#!/bin/bash

saved=$(stty -g)
trap "stty '$saved'" INT TERM EXIT

target=$(ls-interactive 0 3>&1 1>/dev/tty 2>/dev/tty)
[[ -n "$target" ]] && cd "$target"
