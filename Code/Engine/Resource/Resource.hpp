#pragma once
#include <functional>
#include <string>

struct ResourceLocation
{
public:
    ResourceLocation(const std::string& _namespace, const std::string& _path);
    ResourceLocation(const std::string& resourceString); // "namespace:path"

    const std::string& GetNamespace() const { return m_namespace; }
    const std::string& GetPath() const { return m_path; }
    std::string        ToString() const { return m_namespace + ":" + m_path; }

    bool operator==(const ResourceLocation& other) const;
    bool operator<(const ResourceLocation& other) const;

private:
    std::string m_namespace;
    std::string m_path;
};

/// The Resource basic interface
/// @tparam T The subclass of IResource
template <typename T>
class IResource
{
public:
    virtual                  ~IResource() = default;
    virtual ResourceLocation GetResourceLocation() const = 0;
    virtual bool             IsLoaded() const = 0;
    virtual void             Unload() = 0;
    virtual T*               GetResource() = 0;
};

/// 
/// @tparam T The Resource Extended Supplier, resource type bounded 
template <typename T>
class ResourceSupplier
{
private:
    std::function<T*()> m_supplier;
    mutable T*          m_cachedResource = nullptr;
    mutable bool        m_isCached       = false;

public:
    ResourceSupplier(std::function<T*()> supplier) : m_supplier(supplier)
    {
    }

    T* Get() const
    {
        if (!m_isCached)
        {
            m_cachedResource = m_supplier();
            m_isCached       = true;
        }
        return m_cachedResource;
    }

    void Invalidate()
    {
        m_isCached       = false;
        m_cachedResource = nullptr;
    }
};
