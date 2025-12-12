#ifndef INCLUDE_LWG_REPORT_GENERATOR_H
#define INCLUDE_LWG_REPORT_GENERATOR_H

#include <string>
#include <span>
#include <filesystem>

#include "issues.h"  // cannot forward declare the 'section_map' alias, nor the 'LwgIssuesXml' alias

namespace fs = std::filesystem;

namespace lwg
{
struct issue;
struct mailing_info;


struct report_generator {

   report_generator(mailing_info const & info, section_map & sections)
      : lwg_issues_xml(info)
      , section_db(sections)
   {
   }

   // Functions to make the 3 standard published issues list documents
   // A precondition for calling any of these functions is that the list of issues is sorted in numerical order, by issue number.
   // While nothing disastrous will happen if this precondition is violated, the published issues list will list items
   // in the wrong order.
   void make_active(std::span<const issue> issues, fs::path const & path, std::string const & diff_report);

   void make_defect(std::span<const issue> issues, fs::path const & path, std::string const & diff_report);

   void make_closed(std::span<const issue> issues, fs::path const & path, std::string const & diff_report);

   // Additional non-standard documents, useful for running LWG meetings
   void make_tentative(std::span<const issue> issues, fs::path const & path);
      // publish a document listing all tentative issues that may be acted on during a meeting.


   void make_unresolved(std::span<const issue> issues, fs::path const & path);
      // publish a document listing all non-tentative, non-ready issues that must be reviewed during a meeting.

   void make_immediate(std::span<const issue> issues, fs::path const & path);
      // publish a document listing all non-tentative, non-ready issues that must be reviewed during a meeting.

   void make_ready(std::span<const issue> issues, fs::path const & path);
      // publish a document listing all ready issues for a formal vote

   void make_sort_by_num(std::span<issue> issues, fs::path const & filename);

   void make_sort_by_priority(std::span<issue> issues, fs::path const & filename);

   void make_sort_by_status(std::span<issue> issues, fs::path const & filename);

   void make_sort_by_status_mod_date(std::span<issue> issues, fs::path const & filename);

   void make_sort_by_section(std::span<issue> issues, fs::path const & filename, bool active_only = false);

   void make_editors_issues(std::span<const issue> issues, fs::path const & path);

   void make_individual_issues(std::span<const issue> issues, fs::path const & path);

private:
   void make_sort_by_status_impl(std::span<issue> issues, fs::path const & filename, std::string title);

   mailing_info const & lwg_issues_xml;
   section_map &        section_db;
};

} // close namespace lwg

#endif // INCLUDE_LWG_REPORT_GENERATOR_H
