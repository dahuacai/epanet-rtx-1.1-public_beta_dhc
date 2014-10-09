//
//  rtxExceptions.h
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#ifndef epanet_rtx_rtxExceptions_h
#define epanet_rtx_rtxExceptions_h

#include <stdexcept>

namespace RTX {
  
  class RtxException : public std::exception {
  public:
    RtxException(std::string ss = "Unknown") : s(ss) {}
    virtual const char* what() const throw()
    { return s.c_str(); }
    virtual ~RtxException() throw() {}
    std::string s;
  };
  
  class RtxIoException : public RtxException {
  public:
    virtual const char* what() const throw()
    { return "I/O Exception\n"; }
  };
  
  class RtxMethodNotValid : public RtxException {
  public:
    virtual const char* what() const throw()
    { return "Method not Valid\n"; }
  };
  
  class IncompatibleComponent : public RtxException {
    virtual const char* what() const throw()
    { return "Component not compatible\n"; }
  };
}


#endif
