#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include "status.h"

#include <string>
#include <stdexcept>
#include <iostream>  // eases debugging
#include <algorithm>

namespace {
constexpr char const * LWG_ACTIVE {"lwg-active.html" };
constexpr char const * LWG_CLOSED {"lwg-closed.html" };
constexpr char const * LWG_DEFECTS{"lwg-defects.html"};
}

auto lwg::filename_for_status(std::string_view stat) -> std::string_view {
   // Tentative issues are always active
   if(is_tentative(stat)) {
      return LWG_ACTIVE;
   }

   stat = remove_qualifier(stat);
   return (stat == "TC1")           ? LWG_DEFECTS
        : (stat == "CD1")           ? LWG_DEFECTS
        : (stat == "TS")            ? LWG_DEFECTS
        : (stat == "C++11")         ? LWG_DEFECTS
        : (stat == "C++14")         ? LWG_DEFECTS
        : (stat == "C++17")         ? LWG_DEFECTS
        : (stat == "C++20")         ? LWG_DEFECTS
        : (stat == "C++23")         ? LWG_DEFECTS
        : (stat == "WP")            ? LWG_DEFECTS
        : (stat == "Resolved")      ? LWG_DEFECTS
        : (stat == "DR")            ? LWG_DEFECTS
        : (stat == "TRDec")         ? LWG_DEFECTS
        : (stat == "Dup")           ? LWG_CLOSED
        : (stat == "NAD")           ? LWG_CLOSED
        : (stat == "NAD Future")    ? LWG_CLOSED
        : (stat == "NAD Editorial") ? LWG_CLOSED
        : (stat == "NAD Concepts")  ? LWG_CLOSED
        : (stat == "NAD Arrays")    ? LWG_CLOSED
        : (stat == "Voting")        ? LWG_ACTIVE
        : (stat == "Immediate")     ? LWG_ACTIVE
        : (stat == "Ready")         ? LWG_ACTIVE
        : (stat == "Review")        ? LWG_ACTIVE
        : (stat == "New")           ? LWG_ACTIVE
        : (stat == "Open")          ? LWG_ACTIVE
        : (stat == "EWG")           ? LWG_ACTIVE
        : (stat == "LEWG")          ? LWG_ACTIVE
        : (stat == "Core")          ? LWG_ACTIVE
        : (stat == "SG1")           ? LWG_ACTIVE
        : (stat == "SG9")           ? LWG_ACTIVE
        : (stat == "SG16")          ? LWG_ACTIVE
        : (stat == "Deferred")      ? LWG_ACTIVE
        : throw std::runtime_error("unknown status '" + std::string(stat) + "'");
}

auto lwg::is_active(std::string_view stat) -> bool {
   return filename_for_status(stat) == LWG_ACTIVE;
}

auto lwg::is_active_not_ready(std::string_view stat) -> bool {
   return is_active(stat)  and  stat != "Ready";
}

auto lwg::is_defect(std::string_view stat) -> bool {
   return filename_for_status(stat) == LWG_DEFECTS;
}

auto lwg::is_closed(std::string_view stat) -> bool {
   return filename_for_status(stat) == LWG_CLOSED;
}

auto lwg::is_tentative(std::string_view stat) -> bool {
   return stat.starts_with("Tentatively");
}

auto lwg::is_assigned_to_another_group(std::string_view stat) -> bool {
   for( auto s : {"Core", "EWG", "LEWG", "SG1", "SG9", "SG16" }) {
     if(s == stat) return true;
   }
   return false;
}

auto lwg::is_not_resolved(std::string_view stat) -> bool {
   if (is_assigned_to_another_group(stat)) return true;
   for( auto s : {"Deferred", "New", "Open", "Review"}) { if(s == stat) return true; }
   return false;
}

auto lwg::is_votable(std::string_view stat) -> bool {
   stat = remove_tentatively(stat);
   for( auto s : {"Immediate", "Voting"}) { if(s == stat) return true; }
   return false;
}

auto lwg::is_ready(std::string_view stat) -> bool {
   return "Ready" == remove_tentatively(stat);
}

// Functions to "normalize" a status string
namespace {
auto remove_prefix(std::string_view str, std::string_view prefix) -> std::string_view {
   if (str.starts_with(prefix)) {
      str.remove_prefix(prefix.size() + 1);
   }
   return str;
}
}

auto lwg::remove_pending(std::string_view stat) -> std::string_view {
   return remove_prefix(stat, "Pending");
}

auto lwg::remove_tentatively(std::string_view stat) -> std::string_view {
   return remove_prefix(stat, "Tentatively");
}

auto lwg::remove_qualifier(std::string_view stat) -> std::string_view {
   return remove_tentatively(remove_pending(stat));
}

auto lwg::get_status_priority(std::string_view stat) noexcept -> std::ptrdiff_t {
   static std::string_view const status_priority[] {
      "Voting",
      "Tentatively Voting",
      "Immediate",
      "Ready",
      "Tentatively Ready",
      "Tentatively NAD Editorial",
      "Tentatively NAD Future",
      "Tentatively NAD",
      "Review",
      "New",
      "Open",
      "LEWG",
      "EWG",
      "Core",
      "SG1",
      "SG9",
      "SG16",
      "Deferred",
      "Tentatively Resolved",
      "Pending DR",
      "Pending WP",
      "Pending Resolved",
      "Pending NAD Future",
      "Pending NAD Editorial",
      "Pending NAD",
      "NAD Future",
      "DR",
      "WP",
      "C++23",
      "C++20",
      "C++17",
      "C++14",
      "C++11",
      "CD1",
      "TC1",
      "Resolved",
      "TS",
      "TRDec",
      "NAD Editorial",
      "NAD",
      "Dup",
      "NAD Concepts",
      "NAD Arrays",
   };


#if !defined(DEBUG_SUPPORT)
   static auto const first = std::begin(status_priority);
   static auto const last  = std::end(status_priority);
   return std::find_if( first, last, [&](std::string_view str){ return str == stat; } ) - first;
#else
   // Diagnose when unknown status strings are passed
   static auto const first = std::begin(status_priority);
   static auto const last  = std::end(status_priority);
   auto const i = std::find_if( first, last, [&](std::string_view str){ return str == stat; } );
   if(last == i) {
      std::cout << "Unknown status: " << stat << std::endl;
   }
   return i - first;
#endif
}
