// This program resets the status attribute of a single issue
// It relies entirely on textual search/replace and does not
// use any other associated functionality of the list management
// tools.

// standard headers
#include <algorithm>
#include <fstream>
//#include <functional>
#include <iostream>
//#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <format>
#include <cstring>

#include <filesystem>
namespace fs = std::filesystem;

// solution specific headers
//#include "issues.h"
//#include "sections.h"
#include "status.h"

struct bad_issue_file : std::runtime_error {
   bad_issue_file(fs::path const & filename, std::string const & error_message)
      : runtime_error{"Error parsing issue file " + filename.string() + ": " + error_message}
      {
   }
};


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

// ============================================================================================================

void check_is_directory(fs::path const & directory) {
   if (!is_directory(directory)) {
      throw std::runtime_error(directory.string() + " is not an existing directory");
   }
}

int main(int argc, char const * argv[]) {
   try {
//       bool trace_on{false};  // Will pick this up from the command line later

      if (argc != 3 && argc != 4) {
         std::cerr << "Must specify exactly one issue, followed by its new status, followed by an optional description.\n";
//         for (auto arg : argv) {
         for (int i{0}; argc != i;  ++i) {
            char const * arg = argv[i];
            std::cerr << "\t" << arg << "\n";
         }
         return -2;
      }

      int const issue_number = atoi(argv[1]);

      std::string new_status{argv[2]};
      std::replace(new_status.begin(), new_status.end(), '_', ' ');  // simplifies unix shell scripting

      std::string const descr = argc == 4 ? argv[3] : std::string{};

      fs::path path = fs::current_path();

      check_is_directory(path);

      std::string issue_file = std::string{"issue"} + argv[1] + ".xml";
      auto const filename = path / "xml" / issue_file;

      auto issue_data = read_file_into_string(filename);

      // find 'status' tag and replace it
      auto k = issue_data.find("<issue num=\"");
      if (k == std::string::npos) {
         throw bad_issue_file{filename, "Unable to find issue number"};
      }
      k += sizeof("<issue num=\"") - 1;
      auto l = issue_data.find('"', k);
      if (l == std::string::npos) {
         throw bad_issue_file{filename, "Corrupt issue number attribute"};
      }
      if (std::stod(issue_data.substr(k, k-l)) != issue_number) {
         throw bad_issue_file{filename, "Issue number does not match filename"};
      }

      k = issue_data.find("status=\"");
      if (k == std::string::npos) {
         throw bad_issue_file{filename, "Unable to find issue status"};
      }
      k += sizeof("status=\"") - 1;
      l = issue_data.find('"', k);
      if (l == std::string::npos) {
         throw bad_issue_file{filename, "Corrupt status attribute"};
      }
      auto old_status = issue_data.substr(k, l-k);

      // Do not allow an IS status for TS issues, and vice versa.
      if (new_status.starts_with("C++") or new_status == "TS")
      {
         auto t1 = issue_data.find("<title>", l);
         auto t2 = issue_data.find("</title>", l);
         if (t1 == std::string::npos or t2 == std::string::npos or t2 < t1)
            throw bad_issue_file{filename, "Cannot find <title> element"};
         t1 += std::strlen("<title>");
         std::string_view title(issue_data);
         title = title.substr(t1, t2 - t1);
         bool is_ts_issue = false;
         if (title.starts_with('['))
         {
            auto end = title.find(']');
            if (end != title.npos)
            {
               auto tag = title.substr(0, end);
#ifdef __cpp_lib_string_contains
               is_ts_issue = tag.contains(".ts");
#else
               is_ts_issue = tag.find(".ts") != tag.npos;
#endif
            }
         }
         if (is_ts_issue != (new_status == "TS"))
            throw std::runtime_error{std::format("Refusing to set status \"{}\" for issue in {}:\n\t{}: {}", new_status, is_ts_issue ? "a TS" : "the IS", issue_number, title)};
      }

      issue_data.replace(k, l-k, new_status);

      if (lwg::filename_for_status(new_status) != lwg::filename_for_status(old_status)) {
         // when performing a major status change, record the date and change as a note
         auto eod = issue_data.find("</discussion>");
         if (eod == std::string::npos) {
             throw bad_issue_file{filename, "Unable to find end of discussion"};
         }
         std::ostringstream note;
         note << "<note>" << std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
         if (descr.size()) {
            note << ' ' << descr;
            if (descr.back() != '.')
               note << '.';
         }
         note << " Status changed: " + old_status + " &rarr; " + new_status + ".</note>\n";
         issue_data.insert(eod, note.str());
      }

      std::ofstream out_file{filename};
      if (!out_file.is_open()) {
         throw std::runtime_error{"Unable to re-open file " + filename.string()};
      }

      out_file << issue_data;
   }
   catch(std::exception const & ex) {
      std::cout << ex.what() << std::endl;
      return -1;
   }
}
