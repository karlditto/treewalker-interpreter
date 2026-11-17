#!/usr/bin/env bash

PASS=0
FAIL=0
RED=$'\e[38;5;196m'
GREEN=$'\e[38;5;46m'
RESET=$'\e[0m'
prefix="$(dirname $0)"

testcase() {
  local res="$("$prefix"/main "$1")"
  if [[ "$res" = $2 ]]; then
    return 0
  else
    return 1
  fi
}

# test simple arithmetic
# for expression this simple, testing one operator is enough
txt='4+1'
read -r -d '' target <<-EOF
| |+
|  |4
|  |1
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output: "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='4/1/6'
read -r -d '' target <<-EOF
| |/
|  |/
|   |4
|   |1
|  |6
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output: "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

# compound expression mixed with different binding power
txt='4+1 * 4'
read -r -d '' target <<-EOF
| |+
|  |4
|  |*
|   |1
|   |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='3*4+2'
read -r -d '' target <<-EOF
| |+
|  |*
|   |3
|   |4
|  |2
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='3*4+2+4'
read -r -d '' target <<-EOF
| |+
|  |+
|   |*
|    |3
|    |4
|   |2
|  |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='3*4*2+4'
read -r -d '' target <<-EOF
| |+
|  |*
|   |*
|    |3
|    |4
|   |2
|  |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='3+4*2+4'
read -r -d '' target <<-EOF
| |+
|  |+
|   |3
|   |*
|    |4
|    |2
|  |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='3+4+2*4'
read -r -d '' target <<-EOF
| |+
|  |+
|   |3
|   |4
|  |*
|   |2
|   |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

# test assign and comma operator
txt='a=4+2*4'
read -r -d '' target <<-EOF
| |=
|  |a
|  |+
|   |4
|   |*
|    |2
|    |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

# NOTE: chained assign should work fine, this equals to a=(b=1)
txt='a=b=1'
read -r -d '' target <<-EOF
| |=
|  |a
|  |=
|   |b
|   |1
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a=(b=1)'
read -r -d '' target <<-EOF
| |=
|  |a
|  |=
|   |b
|   |1
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='(a=b)=1'
read -r -d '' target <<-EOF
| |=
|  |=
|   |a
|   |b
|  |1
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a+b=1'
read -r -d '' target <<-EOF
| |=
|  |+
|   |a
|   |b
|  |1
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a+b=3+4'
read -r -d '' target <<-EOF
| |=
|  |+
|   |a
|   |b
|  |+
|   |3
|   |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a,3'
read -r -d '' target <<-EOF
| |,
|  |a
|  |3
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a=1,3'
read -r -d '' target <<-EOF
| |,
|  |=
|   |a
|   |1
|  |3
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a=(1,b)'
read -r -d '' target <<-EOF
| |=
|  |a
|  |,
|   |1
|   |b
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='a=(c,b=4)'
read -r -d '' target <<-EOF
| |=
|  |a
|  |,
|   |c
|   |=
|    |b
|    |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

# test prefix
txt='-(c,b=4)'
read -r -d '' target <<-EOF
| |-
|  |,
|   |c
|   |=
|    |b
|    |4
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

txt='-+--33 + -(3) - -3 *+- (6* (4--6))'
read -r -d '' target <<-EOF
| |-
|  |+
|   |-
|    |+
|     |-
|      |-
|       |33
|   |-
|    |3
|  |*
|   |-
|    |3
|   |+
|    |-
|     |*
|      |6
|      |-
|       |4
|       |-
|        |6
EOF
testcase "$txt" "$target"
if [[ $? -eq 0 ]]; then
  ((PASS += 1))
else
  echo test "$txt" failed
  echo AST output:
  echo "$("$prefix"/main "$txt")"
  ((FAIL += 1))
fi

echo -e PASS:"$GREEN$PASS$RESET" FAIL:"$RED$FAIL$RESET"
