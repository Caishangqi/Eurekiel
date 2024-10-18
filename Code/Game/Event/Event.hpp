#pragma once
#include <typeinfo>

struct Event
{
public:
    virtual const std::type_info& getType() const = 0;
    virtual                       ~Event() = default;
};

// 模板化的事件类，自动实现 getType()
template <typename T>
struct TypedEvent : public Event
{
public:
    const std::type_info& getType() const override
    {
        return typeid(T);
    }
};
