#include "Node.hpp"

#include "Config.hpp"

bool Node::getBoolean() const
{
    try
    {
        return std::get<bool>(_value);
    }
    catch (const std::bad_variant_access &a)
    {
        throw NodeExceptionGetBoolean();
    }
}

size_t Node::getInteger() const
{
    try
    {
        return std::get<size_t>(_value);
    }
    catch (const std::bad_variant_access &a)
    {
        throw NodeExceptionGetInteger();
    }
}

std::string Node::getString() const
{
    try
    {
        return std::get<std::string>(_value);
    }
    catch (const std::bad_variant_access &a)
    {
        throw NodeExceptionGetString();
    }
}

std::vector<Node> Node::getArray() const
{
    try
    {
        return std::get<std::vector<Node>>(_value);
    }
    catch (const std::bad_variant_access &a)
    {
        throw NodeExceptionGetArray();
    }
}

std::map<std::string, Node> Node::getObject() const
{
    try
    {
        return std::get<std::map<std::string, Node>>(_value);
    }
    catch (const std::bad_variant_access &a)
    {
        throw NodeExceptionGetObject();
    }
}

void Node::setBoolean(bool boolean)
{
    _value = boolean;
}

void Node::setInteger(size_t integer)
{
    _value = integer;
}

void Node::setString(const std::string string)
{
    _value = string;
}

void Node::setArray(std::vector<Node> array)
{
    _value = array;
}

void Node::setObject(const std::map<std::string, Node> object)
{
    _value = object;
}
