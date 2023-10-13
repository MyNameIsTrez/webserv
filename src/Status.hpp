#pragma once

namespace Status
{
	// TODO: Add more status code from here:
	// https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
	enum Status
	{
		OK = 200,
		BAD_REQUEST = 400,
		FORBIDDEN = 403,
		METHOD_NOT_ALLOWED = 405,
	};
}
