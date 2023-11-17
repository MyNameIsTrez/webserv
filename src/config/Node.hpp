#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct Node
{
public:
	bool getBoolean() const;
	size_t getInteger() const;
	std::string getString() const;
	std::vector<Node> getArray() const;
	std::map<std::string, Node> getObject() const;

	void setBoolean(bool boolean);
	void setInteger(size_t integer);
	void setString(const std::string string);
	void setArray(std::vector<Node> array);
	void setObject(const std::map<std::string, Node> object);

	struct NodeException : public std::runtime_error
	{
		NodeException(const std::string &message) : std::runtime_error(message){};
	};

private:
	std::variant<bool, size_t, std::string, std::vector<Node>, std::map<std::string, Node>> _value;

	struct NodeExceptionGetBoolean : public NodeException
	{
		NodeExceptionGetBoolean() : NodeException("Node exception: Attempt to get boolean failed"){};
	};
	struct NodeExceptionGetInteger : public NodeException
	{
		NodeExceptionGetInteger() : NodeException("Node exception: Attempt to get integer failed"){};
	};
	struct NodeExceptionGetString : public NodeException
	{
		NodeExceptionGetString() : NodeException("Node exception: Attempt to get string failed"){};
	};
	struct NodeExceptionGetArray : public NodeException
	{
		NodeExceptionGetArray() : NodeException("Node exception: Attempt to get array failed"){};
	};
	struct NodeExceptionGetObject : public NodeException
	{
		NodeExceptionGetObject() : NodeException("Node exception: Attempt to get object failed"){};
	};
};
