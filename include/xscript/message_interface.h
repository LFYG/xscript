#ifndef _XSCRIPT_MESSAGE_INTERFACE_H_
#define _XSCRIPT_MESSAGE_INTERFACE_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

namespace xscript {

class MessageParamBase {
public:
    virtual ~MessageParamBase() {};
protected:
    MessageParamBase() {}
};

template <typename Type>
class MessageParam : public MessageParamBase {
public:
    MessageParam(Type *value) : value_(value) {}
    ~MessageParam() {}
    
    Type* value() const {
        return value_;
    }
private:
    Type* value_;
};

class MessageParams {
public:
    MessageParams();
    ~MessageParams();
    
    template <typename Type>
    void addParam(MessageParam<Type>* param) {
        params_.push_back(param);
    }
    
    template <typename Type>
    Type* getParam(unsigned int n) const {
        if (n >= params_.size()) {
            return NULL;
        }
        MessageParam<Type>* casted_param = dynamic_cast<MessageParam<Type>*>(params_[n]);
        return casted_param ? casted_param->value() : NULL;
    }
    
    unsigned int size() const;
private:
    std::vector<MessageParamBase*> params_;
};

class MessageResultBase {
public:
    MessageResultBase();
    virtual ~MessageResultBase();
};

template <typename Type>
class MessageResult : public MessageResultBase {
public:
    MessageResult() {}
    MessageResult(const Type &val) : result_(val)
    {}
    ~MessageResult() {}
    
    void set(const Type &val) {
        result_ = val;
    }
    
    Type& get() {
        return result_;
    }
private:
    Type result_;
};

class MessageHandler {
public:
    MessageHandler();
    virtual ~MessageHandler();

    virtual int process(const MessageParams &params, MessageResultBase &result) = 0;
};

class MessageProcessor {
public:
    ~MessageProcessor();

    static MessageProcessor* instance();
    
    void registerFront(const std::string &key, boost::shared_ptr<MessageHandler> handler);
    void registerBack(const std::string &key, boost::shared_ptr<MessageHandler> handler);
    
    void process(const std::string &key, const MessageParams &params, MessageResultBase &result);
private:
    MessageProcessor();
    
private:
    typedef std::list<boost::shared_ptr<MessageHandler> > HandlerList;
    std::map<std::string, HandlerList> handlers_;
};

} // namespace xscript

#endif // _XSCRIPT_MESSAGE_INTERFACE_H_
