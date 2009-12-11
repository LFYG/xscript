#ifndef _XSCRIPT_STANDARD_DOC_POOL_H_
#define _XSCRIPT_STANDARD_DOC_POOL_H_

#include <string>
#include <map>
#include <list>

#include <boost/utility.hpp>
#include <boost/thread/mutex.hpp>

#include "xscript/tag.h"
#include "xscript/doc_cache_strategy.h"
#include "xscript/xml_helpers.h"
#include "xscript/cache_counter.h"

namespace xscript {

class AverageCounter;

/**
 * Class for storing documents in memory.
 * Combination of hash and LRU list. Will store documents available
 * by key lookup. On "overflow" use LRU strategy to discard old documents.
 */
class DocPool : private boost::noncopyable {
public:
    /**
     * Construct pool
     * \param capacity Maximum number of documents to store.
     * \param name Tag name for statistic gathering.
     */
    DocPool(size_t capacity, const std::string& name);
    virtual ~DocPool();

    /**
     * Result of loading document.
     */
    enum LoadResult {
        LOAD_SUCCESSFUL,
        LOAD_NOT_FOUND,
        LOAD_EXPIRED,
        LOAD_NEED_PREFETCH,
    };

    bool loadDoc(const TagKey &key, Tag &tag, XmlDocSharedHelper &doc);
    LoadResult loadDocImpl(const std::string &keyStr, Tag &tag, XmlDocSharedHelper &doc);

    /**
     * Result of saving doc.
     */
    enum SaveResult {
        SAVE_STORED,
        SAVE_UPDATED,
    };

    bool saveDoc(const TagKey &key, const Tag& tag, const XmlDocSharedHelper &doc);
    SaveResult saveDocImpl(const std::string &keyStr, const Tag& tag, const XmlDocSharedHelper &doc);

    void clear();

    const CacheCounter* getCounter() const;

private:
    class DocData;
    typedef std::map<std::string, DocData> Key2Data;
    typedef std::list<Key2Data::iterator> LRUList;

    class DocData {
    public:
        DocData();
        explicit DocData(LRUList::iterator list_pos);

        void assign(const Tag &tag, const XmlDocSharedHelper &elem);

//        xmlDocPtr copyDoc() const;

        void clearDoc();

    public:
        Tag                 tag;
        XmlDocSharedHelper  doc;
        LRUList::iterator   pos;
        time_t              stored_time;
        bool                prefetch_marked;
    };

    void shrink();
    void removeExpiredDocuments();

    void saveAtIterator(const Key2Data::iterator &i, const Tag &tag, const XmlDocSharedHelper &doc);

private:
    size_t          capacity_;
    std::auto_ptr<CacheCounter>     counter_;

    boost::mutex    mutex_;

    Key2Data        key2data_;
    LRUList         list_;
};


} // namespace xscript

#endif // _XSCRIPT_STANDARD_DOC_POOL_H_
