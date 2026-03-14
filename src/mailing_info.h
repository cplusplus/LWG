#ifndef INCLUDE_LWG_MAILING_INFO_H
#define INCLUDE_LWG_MAILING_INFO_H

#include <iosfwd>
#include <string>
#include <string_view>
#include <span>

namespace lwg
{

struct issue;

struct mailing_info {
   explicit mailing_info(std::istream & stream);

   auto get_doc_number(std::string doc) const -> std::string_view;
   auto get_intro(std::string_view doc) const -> std::string_view;
   auto get_maintainer() const -> std::string;
   auto get_revision() const -> std::string_view;
   auto get_revisions(std::span<const issue> issues, std::string const & diff_report) const -> std::string;
   auto get_statuses() const -> std::string_view;
   auto get_date() const -> std::string_view;
   auto get_title() const -> std::string_view;

private:
   auto get_attribute(std::string_view attribute_name) const -> std::string_view;
      // Return the value of the first xml attibute having the specified 'attribute_name'
      // in the stored XML string, 'm_data', without regard to which element holds that
      // attribute.

   std::string m_data;
      // 'm_data' is reparsed too many times in practice, and memory use is not a major concern.
      // Should cache each of the reproducible calls in additional member strings, either at
      // construction, or lazily on each function eval, checking if the cached string is 'empty'.
      // Note that 'm_data' is immutable, we can use string_view with confidence.
      // However, we do not mark it as 'const', as it would kill the implicit move constructor.
};

}

#endif // INCLUDE_LWG_MAILING_INFO_H
