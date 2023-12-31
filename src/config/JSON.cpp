#include "JSON.hpp"

#include "../Utils.hpp"
#include "Config.hpp"

JSON::JSON(std::istream &file, int max_depth) : _tokenizer(file), _max_depth(max_depth), _depth()
{
    const Token token = _tokenizer.getToken();
    if (token.type != Token::OBJECT_OPEN)
        throw JSONExceptionExpectedObject();
    root = _parseObject();
}

Node JSON::_parseBoolean()
{
    Node node;

    Token token = _tokenizer.getToken();
    node.setBoolean(token.type == Token::BOOLEAN_TRUE);

    return node;
}

Node JSON::_parseInteger()
{
    Node node;

    Token token = _tokenizer.getToken();
    size_t integer;
    if (!Utils::parseNumber(token.string, integer, std::numeric_limits<size_t>::max()))
        throw JSONExceptionInvalidInteger();
    node.setInteger(integer);

    return node;
}

Node JSON::_parseString()
{
    Node node;

    Token token = _tokenizer.getToken();
    node.setString(token.string);

    return node;
}

Node JSON::_parseArray()
{
    Node node;

    std::vector<Node> array;

    _depth++;
    if (_depth > _max_depth)
    {
        throw JSONExceptionNestedTooDeep();
    }

    Token token = _tokenizer.getToken();
    if (token.type != Token::ARRAY_CLOSE)
    {
        while (true)
        {
            switch (token.type)
            {
            case Token::INTEGER: {
                _tokenizer.rollBackToken();
                array.push_back(_parseInteger());
                break;
            }
            case Token::STRING: {
                _tokenizer.rollBackToken();
                array.push_back(_parseString());
                break;
            }
            case Token::ARRAY_OPEN: {
                array.push_back(_parseArray());
                break;
            }
            case Token::OBJECT_OPEN: {
                array.push_back(_parseObject());
                break;
            }
            default: {
                throw JSONExceptionUnexpectedToken();
            }
            }

            token = _tokenizer.getToken();
            if (token.type == Token::ARRAY_CLOSE)
            {
                break;
            }

            if (token.type == Token::COMMA)
            {
                token = _tokenizer.getToken();
            }
            else
            {
                throw JSONExceptionExpectedComma();
            }
        }
    }

    _depth--;

    node.setArray(array);

    return node;
}

Node JSON::_parseObject()
{
    Node node;

    std::map<std::string, Node> object;

    _depth++;
    if (_depth > _max_depth)
    {
        throw JSONExceptionNestedTooDeep();
    }

    Token keyToken = _tokenizer.getToken();
    if (keyToken.type != Token::OBJECT_CLOSE)
    {
        while (true)
        {
            if (keyToken.type != Token::STRING)
            {
                throw JSONExceptionExpectedStringKey();
            }

            const std::string &key = keyToken.string;

            if (object.contains(key))
            {
                throw JSONExceptionDuplicateKey();
            }

            if (_tokenizer.getToken().type != Token::COLON)
            {
                throw JSONExceptionExpectedColon();
            }

            Token valueToken = _tokenizer.getToken();

            switch (valueToken.type)
            {
            case Token::BOOLEAN_TRUE:
            case Token::BOOLEAN_FALSE: {
                _tokenizer.rollBackToken();
                std::string str;
                object.emplace(key, _parseBoolean());
                break;
            }
            case Token::INTEGER: {
                _tokenizer.rollBackToken();
                object.emplace(key, _parseInteger());
                break;
            }
            case Token::STRING: {
                _tokenizer.rollBackToken();
                object.emplace(key, _parseString());
                break;
            }
            case Token::ARRAY_OPEN: {
                object.emplace(key, _parseArray());
                break;
            }
            case Token::OBJECT_OPEN: {
                object.emplace(key, _parseObject());
                break;
            }
            default: {
                throw JSONExceptionUnexpectedToken();
            }
            }

            keyToken = _tokenizer.getToken();
            if (keyToken.type == Token::OBJECT_CLOSE)
            {
                break;
            }

            if (keyToken.type == Token::COMMA)
            {
                keyToken = _tokenizer.getToken();
            }
            else
            {
                throw JSONExceptionExpectedComma();
            }
        }
    }

    _depth--;

    node.setObject(object);

    return node;
}
