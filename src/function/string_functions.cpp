//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// string_functions.cpp
//
// Identification: src/function/string_functions.cpp
//
// Copyright (c) 2015-2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "function/string_functions.h"
#include "type/value_factory.h"

#include "common/macros.h"
#include "executor/executor_context.h"

namespace peloton {
namespace function {

uint32_t StringFunctions::Ascii(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                                const char *str, uint32_t length) {
  PL_ASSERT(str != nullptr);
  return length <= 1 ? 0 : static_cast<uint32_t>(str[0]);
}

#define NextByte(p, plen) ((p)++, (plen)--)

bool StringFunctions::Like(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                           const char *t, uint32_t tlen, const char *p,
                           uint32_t plen) {
  PL_ASSERT(t != nullptr);
  PL_ASSERT(p != nullptr);
  if (plen == 1 && *p == '%') return true;

  while (tlen > 0 && plen > 0) {
    if (*p == '\\') {
      NextByte(p, plen);
      if (plen <= 0) return false;
      if (tolower(*p) != tolower(*t)) return false;
    } else if (*p == '%') {
      char firstpat;
      NextByte(p, plen);

      while (plen > 0) {
        if (*p == '%')
          NextByte(p, plen);
        else if (*p == '_') {
          if (tlen <= 0) return false;
          NextByte(t, tlen);
          NextByte(p, plen);
        } else
          break;
      }

      if (plen <= 0) return true;

      if (*p == '\\') {
        if (plen < 2) return false;
        firstpat = tolower(p[1]);
      } else
        firstpat = tolower(*p);

      while (tlen > 0) {
        if (tolower(*t) == firstpat) {
          int matched = Like(ctx, t, tlen, p, plen);

          if (matched != false) return matched;
        }

        NextByte(t, tlen);
      }
      return false;
    } else if (*p == '_') {
      NextByte(t, tlen);
      NextByte(p, plen);
      continue;
    } else if (tolower(*p) != tolower(*t)) {
      return false;
    }
    NextByte(t, tlen);
    NextByte(p, plen);
  }

  if (tlen > 0) return false;

  while (plen > 0 && *p == '%') NextByte(p, plen);
  if (plen <= 0) return true;

  return false;
}

#undef NextByte

StringFunctions::StrWithLen StringFunctions::Substr(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_length, int32_t from, int32_t len) {
  int32_t signed_end = from + len - 1;
  if (signed_end < 0 || str_length == 0) {
    return StringFunctions::StrWithLen{nullptr, 0};
  }

  uint32_t begin = from > 0 ? unsigned(from) - 1 : 0;
  uint32_t end = unsigned(signed_end);

  if (end > str_length) {
    end = str_length;
  }

  if (begin > end) {
    return StringFunctions::StrWithLen{nullptr, 0};
  }

  return StringFunctions::StrWithLen{str + begin, end - begin + 1};
}

StringFunctions::StrWithLen StringFunctions::Repeat(
    executor::ExecutorContext &ctx, const char *str, uint32_t length,
    uint32_t num_repeat) {
  // Determine the number of bytes we need
  uint32_t total_len = ((length - 1) * num_repeat) + 1;

  // Allocate new memory
  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(total_len));

  // Perform repeat
  char *ptr = new_str;
  for (uint32_t i = 0; i < num_repeat; i++) {
    PL_MEMCPY(ptr, str, length - 1);
    ptr += (length - 1);
  }

  // We done
  return StringFunctions::StrWithLen{new_str, total_len};
}

StringFunctions::StrWithLen StringFunctions::LTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // llvm expects the len to include the terminating '\0'
  if (str_len == 1) {
    return StringFunctions::StrWithLen{nullptr, 1};
  }

  str_len -= 1;
  int tail = str_len - 1, head = 0;

  while (head < (int)str_len && strchr(from, str[head]) != nullptr) {
    head++;
  }

  // Determine length and return
  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

StringFunctions::StrWithLen StringFunctions::RTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // llvm expects the len to include the terminating '\0'
  if (str_len == 1) {
    return StringFunctions::StrWithLen{nullptr, 1};
  }

  str_len -= 1;
  int tail = str_len - 1, head = 0;
  while (tail >= 0 && strchr(from, str[tail]) != nullptr) {
    tail--;
  }

  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

StringFunctions::StrWithLen StringFunctions::Trim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len) {
  return BTrim(ctx, str, str_len, " ", 2);
}

StringFunctions::StrWithLen StringFunctions::BTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // Skip the tailing 0
  str_len--;

  if (str_len == 0) {
    return StringFunctions::StrWithLen{str, 1};
  }

  int head = 0;
  int tail = str_len - 1;

  // Trim tail
  while (tail >= 0 && strchr(from, str[tail]) != nullptr) {
    tail--;
  }

  // Trim head
  while (head < (int)str_len && strchr(from, str[head]) != nullptr) {
    head++;
  }

  // Done
  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

uint32_t StringFunctions::Length(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
    UNUSED_ATTRIBUTE const char *str, uint32_t length) {
  PL_ASSERT(str != nullptr);
  return length;
}

char *StringFunctions::Upper(executor::ExecutorContext &ctx, const char *str,
                             const uint32_t length) {
  PL_ASSERT(str != nullptr);

  // Allocate new memory
  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(length));

  for (uint32_t i = 0; i != length; ++i) {
    new_str[i] = toupper(str[i]);
  }
  return new_str;
}

type::Value StringFunctions::_Upper(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 1);
  if (args[0].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }

  executor::ExecutorContext ctx{nullptr};
  char *ret = StringFunctions::Upper(ctx, args[0].GetAs<const char *>(),
                                     args[0].GetLength());

  return type::ValueFactory::GetVarcharValue(ret);
}

char *StringFunctions::Lower(executor::ExecutorContext &ctx, const char *str,
                             const uint32_t length) {
  PL_ASSERT(str != nullptr);

  // Allocate new memory
  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(length));

  for (uint32_t i = 0; i != length; ++i) {
    new_str[i] = tolower(str[i]);
  }
  return new_str;
}

type::Value StringFunctions::_Lower(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 1);
  if (args[0].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }

  executor::ExecutorContext ctx{nullptr};
  char *ret = StringFunctions::Lower(ctx, args[0].GetAs<const char *>(),
                                     args[0].GetLength());

  return type::ValueFactory::GetVarcharValue(ret);
}

StringFunctions::StrWithLen StringFunctions::Concat(
    executor::ExecutorContext &ctx, const char **concat_strings,
    const uint32_t *concat_lengths, const uint32_t num_strings) {
  uint32_t target_size = 0;
  for (uint32_t i = 0; i != num_strings; ++i) {
    // length includes null byte
    target_size += concat_lengths[i] - 1;
  }
  // make room for single null byte
  ++target_size;

  // Allocate memory for target string
  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(target_size));

  PL_MEMCPY(new_str, concat_strings[0], concat_lengths[0]);
  for (uint32_t i = 1; i != num_strings; ++i) {
    // move back pointer by one to overwrite null byte of previous string
    PL_MEMCPY((new_str + concat_lengths[i - 1] - 1), concat_strings[i],
              concat_lengths[i]);
  }

  return StringFunctions::StrWithLen{new_str, target_size};
}

type::Value StringFunctions::_Concat(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 2);
  if (args[0].IsNull() || args[1].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }

  executor::ExecutorContext ctx{nullptr};
  const char *concat_strings[2] = {args[0].GetAs<const char *>(),
                                   args[1].GetAs<const char *>()};
  const uint32_t concat_lengths[2] = {args[0].GetLength(), args[1].GetLength()};
  StringFunctions::StrWithLen ret =
      StringFunctions::Concat(ctx, concat_strings, concat_lengths, 2);

  return type::ValueFactory::GetVarcharValue(ret.str, ret.length);
}

}  // namespace function
}  // namespace peloton
