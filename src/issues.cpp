#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include "issues.h"
#include "metadata.h"
#include "status.h"

#include <algorithm>
#include <cassert>
#include <istream>
#include <ostream>
#include <string>
#include <stdexcept>

#include <iterator>
#include <fstream>
#include <sstream>

#include <iostream>  // eases debugging

#include <ctime>
#include <chrono>

#include <filesystem>
namespace fs = std::filesystem;

namespace {
auto parse_date(std::istream & temp) -> std::chrono::year_month_day {
#if __cpp_lib_chrono >= 201803L
   std::chrono::year_month_day date{};
   if (temp >> std::chrono::parse(" %d %b %Y", date))
      return date;
   throw std::runtime_error{"date format error"};
#else
   int d;
   temp >> d;
   if (temp.fail()) {
      throw std::runtime_error{"date format error"};
   }

   std::string month;
   temp >> month;

   std::map<std::string_view, int> months{
      {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6},
      {"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct",10}, {"Nov",11}, {"Dec",12}
   };

   int y{ 0 };
   temp >> y;
   return std::chrono::year{y}/months[month]/d;
#endif
}

auto report_date_file_last_modified(std::filesystem::path const & filename, lwg::metadata const& meta) -> std::chrono::year_month_day {
   using namespace std::chrono;
   system_clock::time_point t;
   int id = std::stoi(filename.filename().stem().native().substr(5));
   if (auto it = meta.git_commit_times.find(id); it !=  meta.git_commit_times.end())
      t = system_clock::from_time_t(it->second);
   else {
      auto mtime = fs::last_write_time(filename);
#if __cpp_lib_chrono >= 201803L
      t = clock_cast<system_clock>(mtime);
#else
      auto fnow = fs::file_time_type::clock::now();
      t = system_clock::now() - round<seconds>(fnow - mtime);
#endif
   }

   return year_month_day(floor<days>(t));
}

// Replace '<' and '>' and '&' with HTML character references.
// This is used to turn backtick-quoted inline code into valid XML/HTML.
std::string escape_special_chars(std::string s) {
   static const std::vector<std::pair<char, std::string_view>> subs {
      { '&', "&amp;" }, // this has to come first!
      { '<', "&lt;" },
      { '>', "&gt;" },
   };
   for (auto [c, r] : subs)
      for (auto p = s.find(c); p != s.npos; p = s.find(c, p+r.size()))
         s.replace(p, 1, r);
   return s;
}

} // close unnamed namespace

auto lwg::parse_issue_from_file(std::string tx, std::string const & filename,
  lwg::metadata & meta) -> issue {
   auto& section_db = meta.section_db;
   struct bad_issue_file : std::runtime_error {
      bad_issue_file(std::string const & filename, std::string error_message)
         : runtime_error{"Error parsing issue file " + filename + ": " + error_message}
         { }
   };

   // Replace ```code block``` with valid XML.
   for (size_t p = tx.find("\n```\n"); p != tx.npos; p = tx.find("\n```\n", p))
   {
      size_t p2 = tx.find("\n```\n", p + 5);
      if (p2 == tx.npos)
         throw bad_issue_file{filename, "Unmatched ``` code block: " + tx.substr(p, 10)};
      auto code = "\n<pre><code>" + escape_special_chars(tx.substr(p + 5, p2 - p - 5)) + "\n</code></pre>\n";
      tx.replace(p, p2 - p + 5, code);
      p += code.size();
   }

   // Replace inline `code` with valid XML.
   for (size_t p = tx.find('`'); p != tx.npos; p = tx.find('`', p))
   {
      if (tx[p+1] == '`')
      {
         // Some issues use double backtick for ``quotes like this''.
         // We don't want to do anything here.
         p += 2;
         continue;
      }
      size_t p2 = tx.find('`', p + 1);
      if (p2 > tx.find('\n', p + 1))
      {
         // Do not treat "`foo\nbar`" as inline code.
         // Move to the next backtick and check that one.
         p = p2;
         continue;
      }
      // This class attribute is used by the CSS in src/report_generator.cpp
      // so that the backticks are still displayed if this occurs inside a
      // <pre> element (because that always displays in code font anyway).
      auto code = "<code class='backtick'>"
         + escape_special_chars(tx.substr(p + 1, p2 - p - 1))
         + "</code>";
      tx.replace(p, p2 - p + 1, code);
      p += code.size();
   }

   // <tt> is obsolete in HTML5, replace with <code>:
   for (auto p = tx.find("<tt"); p != tx.npos; p = tx.find("<tt", p+5))
      if (tx.at(p+3) == '>' || tx.at(p+3) == ' ')
         tx.replace(p, 3, "<code");
   for (auto p = tx.find("</tt>"); p != tx.npos; p = tx.find("</tt>", p+7))
         tx.replace(p, 5, "</code>");

   issue is;

   // Get issue number
   auto k = tx.find("<issue num=\"");
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue number"};
   }
   k += sizeof("<issue num=\"") - 1;
   auto l = tx.find('\"', k);
   is.num = std::stoi(tx.substr(k, l-k));

   // Get issue status
   k = tx.find("status=\"", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue status"};
   }
   k += sizeof("status=\"") - 1;
   l = tx.find('\"', k);
   is.stat = tx.substr(k, l-k);

   // Get issue title
   k = tx.find("<title>", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue title"};
   }
   k +=  sizeof("<title>") - 1;
   l = tx.find("</title>", k);
   is.title = tx.substr(k, l-k);

   // Extract doc_prefix from title
   if (is.title[0] == '['
     && is.title.find("[CD]") == std::string::npos) // special case for a few titles
   {
      std::string::size_type pos = is.title.find(']');
      if (pos != std::string::npos)
        is.doc_prefix = is.title.substr(1, pos - 1);
//    std::cout << is.doc_prefix << '\n';
   }

   // Get issue sections
   k = tx.find("<section>", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue section"};
   }
   k += sizeof("<section>") - 1;
   l = tx.find("</section>", k);
   while (k < l) {
      k = tx.find('\"', k);
      if (k >= l) {
          break;
      }
      auto k2 = tx.find('\"', k+1);
      if (k2 >= l) {
         throw bad_issue_file{filename, "Unable to find issue section"};
      }
      ++k;
      section_tag tag;
      tag.prefix = is.doc_prefix;
      tag.name = tx.substr(k+1, k2 - k - 2);
//std::cout << "lookup tag=\"" << tag.prefix << "\", \"" << tag.name << "\"\n";
      is.tags.emplace_back(tag);
      if (section_db.find(is.tags.back()) == section_db.end()) {
          section_num num{};
 //         num.num.push_back(100 + 'X' - 'A');
          num.prefix = tag.prefix;
          num.num.push_back(99);
          section_db[is.tags.back()] = num;
      }
      k = k2;
      ++k;
   }

   if (is.tags.empty()) {
      throw bad_issue_file{filename, "Unable to find issue section"};
   }

   // Get submitter
   k = tx.find("<submitter>", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue submitter"};
   }
   k += sizeof("<submitter>") - 1;
   l = tx.find("</submitter>", k);
   is.submitter = tx.substr(k, l-k);

   // Get date
   k = tx.find("<date>", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue date"};
   }
   k += sizeof("<date>") - 1;
   l = tx.find("</date>", k);

   try {
      std::istringstream temp{tx.substr(k, l-k)};
      is.date = parse_date(temp);
   }
   catch(std::exception const & ex) {
      throw bad_issue_file{filename, "date format error"};
   }

   // Get modification date
   is.mod_date = report_date_file_last_modified(filename, meta);

   // Get priority - this element is optional
   k = tx.find("<priority>", l);
   if (k != std::string::npos) {
      k += sizeof("<priority>") - 1;
      l = tx.find("</priority>", k);
      if (l == std::string::npos) {
         throw bad_issue_file{filename, "Corrupt 'priority' element: no closing tag"};
      }
      is.priority = std::stoi(tx.substr(k, l-k));
   }

   // Trim text to <discussion>
   k = tx.find("<discussion>", l);
   if (k == std::string::npos) {
      throw bad_issue_file{filename, "Unable to find issue discussion"};
   }
   tx.erase(0, k);

   // Find out if issue has a proposed resolution
   if (is_active(is.stat)  or  "Pending WP" == is.stat) {
      auto k2 = tx.find("<resolution>", 0);
      if (k2 == std::string::npos) {
         is.has_resolution = false;
      }
      else {
         k2 += sizeof("<resolution>") - 1;
         auto l2 = tx.find("</resolution>", k2);
         is.resolution = tx.substr(k2, l2 - k2);
         if (is.resolution.length() < 15) {
            // Filter small amounts of whitespace between tags, with no actual resolution
            is.resolution.clear();
         }
//         is.has_resolution = l2 - k2 > 15;
         is.has_resolution = !is.resolution.empty();
      }
   }
   else {
      is.has_resolution = true;
   }

   is.text = std::move(tx);
   return is;
}
