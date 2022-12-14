#pragma once
#ifndef STORAGE_CONF_HPP__
#define STORAGE_CONF_HPP__

#include <slsfs.hpp>
#include <memory>
#include <vector>

namespace slsfsdf
{

class storage_conf
{
protected:
    std::vector<std::shared_ptr<slsfs::storage::interface>> hostlist_;
public:
    virtual ~storage_conf() {}
    virtual void init() = 0;
    virtual int  blocksize() = 0;

    void connect()
    {
        for (std::shared_ptr<slsfs::storage::interface> host : hostlist_)
            host->connect();
    }

    template<typename DoFunction>
    void foreach(DoFunction f)
    {
        for (std::shared_ptr<slsfs::storage::interface> host : hostlist_)
            std::invoke(f, host);
    }
};

// must fix in the future
auto get_thread_local_datastorage() -> std::unique_ptr<storage_conf>&
{
    static thread_local std::unique_ptr<storage_conf> datastorage = nullptr;
    return datastorage;
}

void set_thread_local_datastorage(storage_conf * newvalue)
{
    get_thread_local_datastorage().reset(newvalue);
}


} // namespace slsfsdf

#endif // STORAGE_CONF_HPP__
