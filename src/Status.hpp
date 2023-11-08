#pragma once

namespace Status
{
	// TODO: Add more status code from here:
	// https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
	// TODO: Make sure *every* Status enum has an associated string in reason_phrase_table
	enum Status
	{
		OK = 200,
		MOVED_PERMANENTLY = 301,
		MOVED_TEMPORARILY = 302,
		BAD_REQUEST = 400,
		FORBIDDEN = 403,
		NOT_FOUND = 404,
		METHOD_NOT_ALLOWED = 405,
		REQUEST_ENTITY_TOO_LARGE = 413,
		INTERNAL_SERVER_ERROR = 500,
	};
}
