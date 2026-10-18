#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#define RESPONSE_TRY_AGAIN 0xFFFFFFFF
// Focused surface of installed SDK3.12.1: real source validity and fill methods
// are supplied by the production ReadResponse, not a second callback model.
class AsyncAbstractResponse {
 public:
  virtual ~AsyncAbstractResponse()=default;
  virtual bool _sourceValid() const{return false;}
  virtual size_t _fillBuffer(uint8_t *,size_t){return 0;}
 protected:
  int _code=0;
  size_t _contentLength=0;
  std::string _contentType;
};
