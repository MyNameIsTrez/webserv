#pragma once
#ifndef TOKEN_HPP
#define TOKEN_HPP

#include "Config.hpp"

class Token
{
private:
public:
	Token(void);
	Token(Token const &src);
	virtual ~Token(void);
	Token &operator=(Token const &src);
};

#endif
