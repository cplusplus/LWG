#ifndef INCLUDE_LWG_ISSUES_H
#define INCLUDE_LWG_ISSUES_H

// standard headers
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

// solution specific headers
#include "metadata.h"
#include "status.h"

namespace lwg
{
namespace chrono = std::chrono;

struct issue {
   int                        num;            // ID - issue number
   std::string                stat;           // current status of the issue
   std::string                title;          // descriptive title for the issue
   std::string                doc_prefix;     // extracted from title; e.g. filesys.ts
   std::vector<section_tag>   tags;           // section(s) of the standard affected by the issue
   std::string                submitter;      // original submitter of the issue
   chrono::year_month_day     date;           // date the issue was filed
   chrono::year_month_day     mod_date;       // date the issue was last changed
   std::set<std::string>      duplicates;     // sorted list of duplicate issues, stored as html anchor references.
   std::string                text;           // text representing the issue
   int                        priority = 99;  // severity, 1 = critical, 4 = minor concern, 0 = trivial to resolve, 99 = not yet prioritised
   std::string                owner;          // person identified as taking ownership of drafting/progressing the issue
   std::string                resolution;     // extracted resolution text (if any), also present in 'text'
   bool                       has_resolution; // 'true' if 'text' contains a proposed resolution
};

struct order_by_issue_number {
    bool operator()(issue const & x, issue const & y) const noexcept   {  return x.num < y.num;   }
    bool operator()(issue const & x, int y)           const noexcept   {  return x.num < y;       }
    bool operator()(int x,           issue const & y) const noexcept   {  return x     < y.num;   }
};

auto parse_issue_from_file(std::string file_contents, std::string const & filename, lwg::metadata & meta) -> issue;
  // Seems appropriate constructor behavior.
  //
  // Note that 'section_db' is modifiable as new (unknown) sections may be inserted,
  // typically for issues reported against older documents with sections that have
  // since been removed, replaced or merged.
  //
  // The filename is passed only to improve diagnostics.

} // close namespace lwg

#endif // INCLUDE_LWG_ISSUES_H
