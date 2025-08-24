#!/usr/bin/python

# usage: git log --raw --no-show-signature --pretty=%ct | python bin/make_dates.py > dates

import sys
import re

# The input looks like
# 1728481670
#
# :100644 100644 02412851ac 9f5998ba82 M  xml/issue4159.xml
# :100644 100644 fa2274aa17 74485b30c1 M  xml/issue4162.xml
# :100644 100644 8023699cb9 d007f71c08 M  xml/issue4164.xml

mtimes = {}
current_mtime = ''
for l in sys.stdin.readlines():
    l = l.rstrip()
    if not l:
        # blank line
        continue

    # timestamp
    if l[0] != ':':
        current_mtime = l
        continue

    # last piece of line is the file name
    file = l.split()[-1]
    m = re.match('xml/issue(\\d+).xml', file)

    if m: 
        num = int(m[1])
        if num not in mtimes:
            mtimes[num] = current_mtime

for (num, time) in sorted(list(mtimes.items())):
    print(f'{num:04} {time}')
