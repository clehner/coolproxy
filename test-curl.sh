#!/bin/bash

PORT=1080

URL=http://csug.rochester.edu/u/

./coolproxy $PORT >&2 &
PID=$!

if diff <(curl -s "$URL") <(curl -s "$URL" --proxy "localhost:$PORT"); then
	echo 'Success'
	kill $PID
	exit 0
else
	kill $PID
	echo 'Fail' >&2
	exit 1
fi
