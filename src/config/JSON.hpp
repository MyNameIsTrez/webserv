#pragma once

#include "Node.hpp"
#include "Tokenizer.hpp"

class JSON
{
  public:
    JSON(std::istream &file, int max_depth);

    Node root;

    struct JSONException : public std::runtime_error
    {
        JSONException(const std::string &message) : std::runtime_error(message)
        {
        }
    };

  private:
    Node _parseBoolean();
    Node _parseInteger();
    Node _parseString();
    Node _parseArray();
    Node _parseObject();

    struct JSONExceptionExpectedStringKey : public JSONException
    {
        JSONExceptionExpectedStringKey() : JSONException("JSON exception: Expected string key")
        {
        }
    };
    struct JSONExceptionExpectedIntegerKey : public JSONException
    {
        JSONExceptionExpectedIntegerKey() : JSONException("JSON exception: Expected integer key")
        {
        }
    };
    struct JSONExceptionExpectedColon : public JSONException
    {
        JSONExceptionExpectedColon() : JSONException("JSON exception: Expected colon")
        {
        }
    };
    struct JSONExceptionExpectedComma : public JSONException
    {
        JSONExceptionExpectedComma() : JSONException("JSON exception: Expected comma")
        {
        }
    };
    struct JSONExceptionExpectedObject : public JSONException
    {
        JSONExceptionExpectedObject() : JSONException("JSON exception: Expected object")
        {
        }
    };
    struct JSONExceptionInvalidInteger : public JSONException
    {
        JSONExceptionInvalidInteger() : JSONException("JSON exception: Invalid integer")
        {
        }
    };
    struct JSONExceptionUnexpectedToken : public JSONException
    {
        JSONExceptionUnexpectedToken() : JSONException("JSON exception: Unexpected token")
        {
        }
    };
    struct JSONExceptionDuplicateKey : public JSONException
    {
        JSONExceptionDuplicateKey() : JSONException("JSON exception: Duplicate key")
        {
        }
    };
    struct JSONExceptionNestedTooDeep : public JSONException
    {
        JSONExceptionNestedTooDeep() : JSONException("JSON exception: Nested too deep")
        {
        }
    };

    Tokenizer _tokenizer;
    int _max_depth;
    int _depth;
};
