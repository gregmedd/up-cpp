#!/bin/bash

PROJECT_ROOT="$(realpath "$(dirname "$0")/../")"

if [ -n "$(which clang-format-12)" ]; then
    # NOTE: Using clang-format-12 in CI system, too
    FORMATTER=clang-format-12
elif [ -n "$(which clang-format)" ]; then
    echo "Did not find clang-format-12. Trying clang-format. Results may not"
    echo "match formatting in GitHub CI process."
    FORMATTER=clang-format
else
    echo "Could not find clang-format. Please make sure it is installed" 1>&2
    exit 2
fi

echo "Running $FORMATTER on all files in '$PROJECT_ROOT'"
shopt -s globstar

pushd -n "$PROJECT_ROOT"
for f in **/*.h **/*.cpp; do
	echo
	echo "Checking file '$f'"
	$FORMATTER -i "$f"
done
popd -n
