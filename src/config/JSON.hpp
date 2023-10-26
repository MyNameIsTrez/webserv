#pragma once
#ifndef JSON_HPP
#define JSON_HPP

#include "Config.hpp"

class JSON
{
public:
	JSON(std::istream &file);

	Node root;

private:
	Node _parseBoolean();
	Node _parseInteger();
	Node _parseString();
	Node _parseArray();
	Node _parseObject();

	struct JSONException : public std::runtime_error
	{
		JSONException(const std::string &message) : std::runtime_error(message){};
	};
	struct JSONExceptionExpectedStringKey : public JSONException
	{
		JSONExceptionExpectedStringKey() : JSONException("JSON exception: Expected string key"){};
	};
	struct JSONExceptionExpectedIntegerKey : public JSONException
	{
		JSONExceptionExpectedIntegerKey() : JSONException("JSON exception: Expected integer key"){};
	};
	struct JSONExceptionExpectedColon : public JSONException
	{
		JSONExceptionExpectedColon() : JSONException("JSON exception: Expected colon"){};
	};
	struct JSONExceptionExpectedComma : public JSONException
	{
		JSONExceptionExpectedComma() : JSONException("JSON exception: Expected comma"){};
	};
	struct JSONExceptionExpectedObject : public JSONException
	{
		JSONExceptionExpectedObject() : JSONException("JSON exception: Expected object"){};
	};
	struct JSONExceptionInvalidInteger : public JSONException
	{
		JSONExceptionInvalidInteger() : JSONException("JSON exception: Invalid integer"){};
	};
	struct JSONExceptionUnexpectedToken : public JSONException
	{
		JSONExceptionUnexpectedToken() : JSONException("JSON exception: Unexpected token"){};
	};

	Tokenizer _tokenizer;
};

#endif
