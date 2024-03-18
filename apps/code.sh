#!/bin/bash
cleanup_monitor() {
    # Wait for the process named "surf" to exist
    while ! pgrep -x surf > /dev/null; do
        sleep 1
    done

    # Wait until the "surf" process doesn't exist
    while pgrep -x surf > /dev/null; do
        sleep 1
    done

    # Kill all processes named "code"
    pkill code
}

echo $$ > $1

# Start code serve-web and capture output
temp_file=$(mktemp)
code serve-web > "$temp_file" &

# Wait for the process to write to the file (adjust time as needed)
sleep 1

# Extract URL from the output
url=$(grep -o 'http\(s\)\?://\(127\.0\.0\.1\|localhost\):[0-9]\+' "$temp_file" | head -1)

cleanup_monitor &

sleep 1
# Start surf in the foreground
exec surf "$url"
