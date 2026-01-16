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
   std::map<int, std::string> duplicates;     // duplicate issues, as number and formatted html anchor
   std::string                text;           // text representing the issue
   int                        priority = 99;  // severity, 1 = critical, 4 = minor concern, 0 = trivial to resolve, 99 = not yet prioritised
   std::string                owner;          // person identified as taking ownership of drafting/progressing the issue
   std::string                resolution;     // extracted resolution text (if any), also present in 'text'
   bool                       has_resolution; // 'true' if 'text' contains a proposed resolution
};

auto parse_issue_from_file(std::string file_contents, std::string const & filename, lwg::metadata & meta) -> issue;
  // Seems appropriate constructor behavior.
  //
  // Note that 'section_db' is modifiable as new (unknown) sections may be inserted,
  // typically for issues reported against older documents with sections that have
  // since been removed, replaced or merged.
  //
  // The filename is passed only to improve diagnostics.


inline int stoi(const std::string& s)
{
    std::size_t idx = 0;
    try
    {
        return std::stoi(s, &idx);
    }
    catch (const std::out_of_range&)
    {
        throw std::out_of_range("std::stoi: out of range: \"" + s + '"');
    }
    catch (const std::invalid_argument&)
    {
        throw std::invalid_argument("std::stoi: invalid argument: \"" + s + '"');
    }
}

} // close namespace lwg

#endif // INCLUDE_LWG_ISSUES_H
