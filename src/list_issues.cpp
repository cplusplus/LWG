// This program reads all the issues in the issues directory passed as the first command line argument.
// If all documents are successfully parsed, it will generate the standard LWG Issues List documents
// for an ISO SC22 WG21 mailing.

// Based on code originally donated by Howard Hinnant.
// Since modified by Alisdair Meredith and modernised by Jonathan Wakely.

// Note that this program requires a C++17 compiler supporting std::filesystem.

// TODO
// .  Better handling of TR "sections", and grouping of issues in "Clause X"
// .  Sort the Revision comments in the same order as the 'Status' reports, rather than alphabetically
// .  Lots of tidy and cleanup after merging the revision-generating tool
// .  Refactor more common text
// .  Split 'format' function and usage to that the issues vector can pass by const-ref in the common cases
// .  Document the purpose and contract on each function
// .  Waiting on external fix for preserving file-dates
// .  sort-by-last-modified-date should offer some filter or separation to see only the issues modified since the last meeting

// Missing standard facilities that we work around
// . (none at present)

// Missing standard library facilities that would probably not change this program
// . XML parser

// standard headers
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <filesystem>
namespace fs = std::filesystem;

// solution specific headers
#include "issues.h"
#include "metadata.h"


auto read_file_into_string(fs::path const & filename) -> std::string {
   // read a text file completely into memory, and return its contents as
   // a 'string' for further manipulation.

   std::ifstream infile{filename};
   if (!infile.is_open()) {
      throw std::runtime_error{"Unable to open file " + filename.string()};
   }

   std::istreambuf_iterator<char> first{infile}, last{};
   return std::string {first, last};
}

// Issue-list specific functionality for the rest of this file
// ===========================================================

auto is_issue_xml_file(fs::directory_entry const & e) {
   if (e.is_regular_file()) {
      fs::path f = e.path().filename();
      return f.string().starts_with("issue") && f.extension() == ".xml";
   }
   return false;
}

void filter_issues(fs::path const & issues_path, lwg::metadata & meta, std::function<bool(lwg::issue const &)> predicate) {
   // Open the specified directory, 'issues_path', and iterate all the '.xml' files
   // it contains, parsing each such file as an LWG issue document. Collect
   // the number of every issue that satisfies the 'predicate'.

  std::vector<int> nums;
  for (auto ent : fs::directory_iterator(issues_path)) {
     if (is_issue_xml_file(ent)) {
         fs::path const issue_file = ent.path();
        auto const iss = parse_issue_from_file(read_file_into_string(issue_file), issue_file.string(), meta);
        if (predicate(iss)) {
          nums.push_back(iss.num);
        }
     }
  }
  // Write the sorted issue numbers to stdout.
  std::sort(nums.begin(), nums.end());
  for (auto num : nums)
    std::cout << num << '\n';
}

// ============================================================================================================

void check_is_directory(fs::path const & directory) {
   if (!is_directory(directory)) {
      throw std::runtime_error(directory.string() + " is not an existing directory");
   }
}

int main(int argc, char const* argv[]) {
   try {
      if (argc != 2) {
         std::cerr << "Must specify exactly one status\n";
         return 2;
      }
      std::string const status{argv[1]};
  
      fs::path path = fs::current_path();

      check_is_directory(path);

      auto metadata = lwg::metadata::read_from_path(path);

      filter_issues(path / "xml/", metadata, [status](lwg::issue const & iss) { return status == iss.stat; });
   }
   catch(std::exception const & ex) {
      std::cout << ex.what() << std::endl;
      return -1;
   }
}

