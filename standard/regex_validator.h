#ifndef _XSCRIPT_STANDARD_REGEX_VALIDATOR_H_
#define _XSCRIPT_STANDARD_REGEX_VALIDATOR_H_

#include <pcre.h>
#include "xscript/validator.h"
#include "xscript/validator_exception.h"

namespace xscript
{


/**
 * Regex validator. Validate param using regexes.
 * Example of usage:
 *   param type="QueryArg" id="foo" validator="regex" pattern="^\S+$" options="i"
 *
 * Params:
 *   pattern - mandatory regex pattern
 *   options - optional options. Such as "i" for case-insensetive match.
 */
class RegexValidator : public Validator {
public:
    RegexValidator(xmlNodePtr node);
    ~RegexValidator();
    
    /// Static constructor
    static Validator* create(xmlNodePtr node);

    // Real check re. For unit-tests simplification
    bool checkString(const std::string &value) const;

protected:
    virtual void checkImpl(const Context *ctx, const Param &value) const;

private:
    /// Compiled RE
    pcre* re_;

    /// Extra flags for pcre
    pcre_extra re_extra_;
   
    /// PCRE options. At least UTF8 will be set.  
    int pcre_options_;
};



} // namespace xscript

#endif
