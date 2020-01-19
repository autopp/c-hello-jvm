#!/bin/bash

set -eu
out=$(./jvm Hello)
if [ "$out" != "hello world" ]; then
  echo "expected 'hello world', but got '${out}'" >&2
  exit 1
fi
