#include "mailing_info.h"

#include "issues.h"

#include "html_utils.h"

#include <algorithm>
#include <istream>
#include <sstream>

namespace {

void replace_all_irefs(std::span<const lwg::issue> issues, std::string & s) {
   // Replace all tagged "issues references" in string 's' with an HTML anchor-link to the live issue
   // in its appropriate issue list, as determined by the issue's status.
   // Format of an issue reference: <iref ref="ISS"/>
   // Format of anchor: <a href="lwg-INDEX.html#ISS">ISS</a>

   for (auto i = s.find("<iref ref=\""); i != std::string::npos; i = s.find("<iref ref=\"")) {
      auto j = s.find('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"missing '>' after iref"};
      }

      auto k = s.find('\"', i+5);
      if (k >= j) {
         throw std::runtime_error{"missing '\"' in iref"};
      }
      auto l = s.find('\"', k+1);
      if (l >= j) {
         throw std::runtime_error{"missing '\"' in iref"};
      }

      ++k;

      std::istringstream temp{s.substr(k, l-k)};
      int num;
      temp >> num;
      if (temp.fail()) {
         throw std::runtime_error{"bad number in iref"};
      }

      auto n = std::ranges::lower_bound(issues, num, {}, &lwg::issue::num);
      if (n == issues.end() || n->num != num) {
         std::ostringstream er;
         er << "couldn't find number " << num << " in iref";
         throw std::runtime_error{er.str()};
      }

      std::string r{make_html_anchor(*n)};
      j -= i - 1;
      s.replace(i, j, r);
      // i += r.size() - 1;  // unused, copy/paste from elsewhere?
   }
}

} // close unnamed namespace

auto lwg::make_html_anchor(lwg::issue const & iss) -> std::string {
   auto num = std::to_string(iss.num);
   // Remove XML elements from the title to just get the text nodes.
   auto title = lwg::strip_xml_elements(iss.title);
   // Attribute text is delimited with quotes so replace them with "&quot;".
   title = lwg::replace_reserved_char(std::move(title), '"', "&quot;");

   std::string result{"<a href=\""};
   result += filename_for_status(iss.stat);
   result += '#';
   result += num;
   result += "\" title=\"";
   result += title;
   result += " (Status: ";
   result += iss.stat;
   result += ")\">";
   result += num;
   result += "</a>";
   return result;
}

namespace lwg
{

mailing_info::mailing_info(std::istream & stream)
   : m_data{std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}}
   {
}

auto mailing_info::get_doc_number(std::string doc) const -> std::string {
    if (doc == "active") {
        doc = "active_docno";
    }
    else if (doc == "defect") {
        doc = "defect_docno";
    }
    else if (doc == "closed") {
        doc = "closed_docno";
    }
    else {
        throw std::runtime_error{"unknown argument to get_doc_number: " + doc};
    }

    return get_attribute(doc);
}

auto mailing_info::get_intro(std::string doc) const -> std::string {
    if (doc == "active") {
        doc = "<intro list=\"Active\">";
    }
    else if (doc == "defect") {
        doc = "<intro list=\"Defects\">";
    }
    else if (doc == "closed") {
        doc = "<intro list=\"Closed\">";
    }
    else {
        throw std::runtime_error{"unknown argument to intro: " + doc};
    }

    auto i = m_data.find(doc);
    if (i == std::string::npos) {
        throw std::runtime_error{"Unable to find intro in lwg-issues.xml"};
    }
    i += doc.size();
    auto j = m_data.find("</intro>", i);
    if (j == std::string::npos) {
        throw std::runtime_error{"Unable to parse intro in lwg-issues.xml"};
    }
    return m_data.substr(i, j-i);
}


auto mailing_info::get_maintainer() const -> std::string {
   std::string r = get_attribute("maintainer");
   auto m = r.find("&lt;");
   if (m == std::string::npos) {
      throw std::runtime_error{"Unable to parse maintainer email address in lwg-issues.xml"};
   }
   m += sizeof("&lt;") - 1;
   auto me = r.find("&gt;", m);
   if (me == std::string::npos) {
      throw std::runtime_error{"Unable to parse maintainer email address in lwg-issues.xml"};
   }
   std::string email = r.substr(m, me-m);
   // &lt;                                    lwgchair@gmail.com    &gt;
   // &lt;<a href="mailto:lwgchair@gmail.com">lwgchair@gmail.com</a>&gt;
   r.replace(m, me-m, "<a href=\"mailto:" + email + "\">" + email + "</a>");
   return r;
}

auto mailing_info::get_revision() const -> std::string {
   return get_attribute("revision");
}


auto mailing_info::get_revisions(std::span<const issue> issues, std::string const & diff_report) const -> std::string {
   auto i = m_data.find("<revision_history>");
   if (i == std::string::npos) {
      throw std::runtime_error{"Unable to find <revision_history> in lwg-issues.xml"};
   }
   i += sizeof("<revision_history>") - 1;

   auto j = m_data.find("</revision_history>", i);
   if (j == std::string::npos) {
      throw std::runtime_error{"Unable to find </revision_history> in lwg-issues.xml"};
   }
   auto s = m_data.substr(i, j-i);
   j = 0;

   // building a potentially large string, would a stringstream be a better solution?
   // Probably not - string will not be *that* big, and stringstream pays the cost of locales
   std::string r = "<ul>\n";

   r += "<li>";
   r += get_revision() + ": " + get_attribute("date") + " " + get_attribute("title");   // We should date and *timestamp* this reference, as we expect to generate several documents per day
   r += diff_report;
   r += "</li>\n";

   while (true) {
      i = s.find("<revision tag=\"", j);
      if (i == std::string::npos) {
         break;
      }
      i += sizeof("<revision tag=\"") - 1;
      j = s.find('\"', i);
      std::string const rv = s.substr(i, j-i);
      i = j+2;
      j = s.find("</revision>", i);

      r += "<li>";
      r += rv + ": ";
      r += s.substr(i, j-i);
      r += "</li>\n";
   }
   r += "</ul>\n";

   replace_all_irefs(issues, r);

   return r;
}


auto mailing_info::get_statuses() const -> std::string {
   auto i = m_data.find("<statuses>");
   if (i == std::string::npos) {
      throw std::runtime_error{"Unable to find statuses in lwg-issues.xml"};
   }
   i += sizeof("<statuses>") - 1;

   auto j = m_data.find("</statuses>", i);
   if (j == std::string::npos) {
      throw std::runtime_error{"Unable to parse statuses in lwg-issues.xml"};
   }
   return m_data.substr(i, j-i);
}

auto mailing_info::get_date() const -> std::string {
   return get_attribute("date");
}

auto mailing_info::get_title() const -> std::string {
   return get_attribute("title");
}

auto mailing_info::get_attribute(std::string const & attribute_name) const -> std::string {
    std::string search_string{attribute_name + "=\""};
    auto i = m_data.find(search_string);
    if (i == std::string::npos) {
        throw std::runtime_error{"Unable to find " + attribute_name + " in lwg-issues.xml"};
    }
    i += search_string.size();
    auto j = m_data.find('\"', i);
    if (j == std::string::npos) {
        throw std::runtime_error{"Unable to parse " + attribute_name + " in lwg-issues.xml"};
    }
    return m_data.substr(i, j-i);
}

} // close namespace lwg
