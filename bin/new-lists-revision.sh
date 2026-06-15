#!/bin/bash

# Script to prepare a new revision of the official LWG issue lists
# (e.g. "R127 Post-Brno 2026").
# Usage: bin/new-lists-revisions.sh <TITLE>

die()
{
  echo "$0: $*" >&2
  exit 1
}

confirm()
{
  read -p 'Commit changes? [y/N] '
  expr "$REPLY" : y >/dev/null || exit 0
}

set -e

if [ $# -ne 1 ]
then
  die "Missing argument: title for issue list revision"
fi

title="$1"

# This contains the meta-data for the issues lists:
mainfile='xml/lwg-issues.xml'

# Extract the Rnnn revision number from the XML file:
rev=$( sed -n '1s/^<issueslist revision=//p' "$mainfile" | sed 's/"//g' )
expr "$rev" : 'D[1-9][0-9]\+' >/dev/null || die "Revision should be of the form Dnnn"
newrev=R${rev:1}
sed -e '1s/"'$rev'"$/"'$newrev'"/' \
    -e '6s/date="....-..-.."$/date="'$(date +%Y-%m-%d)'"/' \
    -e '7s/title=.*/title="'"$title $(date '+%Y')"'"/' \
    "$mainfile" > "$mainfile.tmp"
mv "$mainfile.tmp" "$mainfile"

# The XML file should contain the new Rnnn number, date, and title.
# Review the diff and commit those changes.
git diff "$mainfile"
if ! git diff --quiet "$mainfile"
then
  echo "New lists will be generated using these values."
  confirm
  git commit -m "$newrev" "$mainfile"
fi

# Generate lwgNNN.zip containing the issue lists and indices by status etc.
make zip-file

rev=$newrev
# Now update lwg-issues.xml with a 'D' revision number, placeholder title for
# the snapshots of the lists, and the revision history for the $rev files.
newrev=D$(expr "${rev:1}" + 1)
sed -e '1s/"'$rev'"$/"'$newrev'"/' \
    -e '7s/title=.*/title="Since '"$(date '+%B %Y')"'"/' \
    -e '/^<revision tag=/,$d' \
    "$mainfile" > "$mainfile.tmp"
# Generate the revision history for the Rnnn revision and add to the XML file:
bin/lists revision history | sed -n '/^<revision/,$p' >> "$mainfile.tmp"
sed '1,/^<revision_history>$/d' "$mainfile" >> "$mainfile.tmp"
mv "$mainfile.tmp" "$mainfile"

# Update the stored copy of the previous TOC, to use next time we generate
# the revision history.
cp mailing/lwg-toc.html meta-data/lwg-toc.old.html
bin/lists

# Review the diff and commit.
git diff "$mainfile" meta-data/lwg-toc.old.html
if ! git diff --quiet "$mainfile" meta-data/lwg-toc.old.html
then
  confirm
  git commit -m "$newrev" "$mainfile" meta-data/lwg-toc.old.html
fi

echo "New lists for publication on open-std.org are in lwg${rev:1}.zip"
