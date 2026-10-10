#include <AccessPolicy.h>
#include <assert.h>
#include <stdio.h>
#include <string>
using namespace BoardCore::Access;
int main() {
  assert(equal("abc", "abc") && !equal("abc", "abC") && !equal("a", "aa"));
  assert(usernameValid("David._-09") && !usernameValid("") && !usernameValid("admin:bad"));
  assert(usernameValid(std::string(32, 'a').c_str()) && !usernameValid(std::string(33, 'a').c_str()));
  assert(passwordValid("abcdefgh") && passwordValid(std::string(64, 'x').c_str()));
  assert(!passwordValid("short") && !passwordValid("abcdefgh\n") && !passwordValid(std::string(65, 'x').c_str()));
  assert(!tokenValid("") && !tokenValid("ABCDEF00000000000000000000000000"));
  Sessions sessions; char token[33];
  for (int i = 0; i < Sessions::kCapacity; ++i) { snprintf(token, sizeof(token), "%032x", i + 1); assert(sessions.add(token, 100)); }
  assert(!sessions.add("ffffffffffffffffffffffffffffffff", 100));
  assert(sessions.validate("00000000000000000000000000000001", 200));
  sessions.revoke("00000000000000000000000000000001");
  assert(!sessions.validate("00000000000000000000000000000001", 201));
  assert(sessions.add("ffffffffffffffffffffffffffffffff", 201));
  assert(!sessions.validate("ffffffffffffffffffffffffffffffff", 201 + Sessions::kIdleMs));
  assert(sessions.add("ffffffffffffffffffffffffffffffff", 201 + Sessions::kIdleMs));
  sessions.clear(); assert(!sessions.validate("ffffffffffffffffffffffffffffffff", 201 + Sessions::kIdleMs));
  assert(sessions.add("ffffffffffffffffffffffffffffffff", 0xffffff00));
  assert(sessions.validate("ffffffffffffffffffffffffffffffff", 0x100));
  assert(!sessions.validate("ffffffffffffffffffffffffffffffff", 0x100 + Sessions::kIdleMs));
  puts("Access credential boundaries, session capacity, revocation, expiry and rollover passed.");
}
