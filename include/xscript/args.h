#ifndef _XSCRIPT_ARG_LIST_H_
#define _XSCRIPT_ARG_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include <boost/cstdint.hpp>
#include <boost/utility.hpp>

namespace xscript {

class Context;
class TaggedBlock;
class TypedValue;

class ArgList : private boost::noncopyable {
public:
    ArgList();
    virtual ~ArgList();

    void addAsChecked(const std::string &type, const std::string &value, bool checked);

    virtual void add(bool value) = 0;
    virtual void add(double value) = 0;
    virtual void add(boost::int32_t value) = 0;
    virtual void add(boost::int64_t value) = 0;
    virtual void add(boost::uint32_t value) = 0;
    virtual void add(boost::uint64_t value) = 0;
    virtual void add(const std::string &value) = 0;
    virtual void addAs(const std::string &type, const std::string &value);
    virtual void addAs(const std::string &type, const TypedValue &value);
    virtual void addState(const Context *ctx);
    virtual void addRequest(const Context *ctx) ;
    virtual void addRequestData(const Context *ctx);
    virtual void addTag(const TaggedBlock *tb, const Context *ctx);

    virtual bool empty() const = 0;
    virtual unsigned int size() const = 0;
    virtual const std::string& at(unsigned int i) const = 0;

    //TODO: add member ArgListData
};

class NilSupportedArgList : public ArgList {
public:
    NilSupportedArgList();
    virtual ~NilSupportedArgList();

    virtual void addNilAs(const std::string &type) = 0;
    virtual const std::string* get(unsigned int i) const = 0;
};

class StringArgList : public NilSupportedArgList {
public:
    StringArgList();
    virtual ~StringArgList();
    virtual void add(bool value);
    virtual void add(double value);
    virtual void add(boost::int32_t value);
    virtual void add(boost::int64_t value);
    virtual void add(boost::uint32_t value);
    virtual void add(boost::uint64_t value);
    virtual void add(const std::string &value);
    virtual bool empty() const;
    virtual unsigned int size() const;
    virtual const std::string& at(unsigned int i) const;

    const std::vector<std::string>& args() const;

    virtual void addNilAs(const std::string &type);
    virtual const std::string* get(unsigned int i) const;

private:
    struct Data;
    std::auto_ptr<Data> data_;
};

class CheckedStringArgList : public StringArgList {
public:
    CheckedStringArgList();
    explicit CheckedStringArgList(bool checked);
    virtual ~CheckedStringArgList();

    virtual void addAs(const std::string &type, const std::string &value);

private:
    struct Data;
    std::auto_ptr<Data> data_;
};


// TODO: inherit from StringArgList
class CommonArgList : public virtual ArgList {
public:
    CommonArgList();
    virtual ~CommonArgList();
    virtual void add(bool value);
    virtual void add(double value);
    virtual void add(boost::int32_t value);
    virtual void add(boost::int64_t value);
    virtual void add(boost::uint32_t value);
    virtual void add(boost::uint64_t value);
    virtual void add(const std::string &value);
    virtual bool empty() const;
    virtual unsigned int size() const;
    virtual const std::string& at(unsigned int i) const;

    const std::vector<std::string>& args() const;

private:
    std::vector<std::string> args_;
};

} // namespace xscript

#endif // _XSCRIPT_ARG_LIST_H_
