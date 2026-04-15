#pragma once

#include <string>

class Object
{
public:
    const std::string& GetName() const
    {
        return m_name;
    }

    void SetName(std::string name)
    {
        m_name = name;
    }

private:
    std::string m_name;
};