#!/bin/sh

# Sweep all issues from Voting -> WP

if [ $# -ne 2 ]
then
  echo "Usage: $0 '<Meeting Name> YYYY-MM-DD'" >&2
  exit 1
fi

today=$(date +%F)
meeting=$1
date=$2

bin/list_issues Voting | while read inum
do
  bin/set_status $inum WP
  sed -i "s/^<note>$today /<note>$meeting $date; /" xml/issue$inum.xml
done
