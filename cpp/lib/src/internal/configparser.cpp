/**************************************************************************
*   Copyright (C) 2010 by Eugene V. Lyubimkin                             *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License                  *
*   (version 3 or above) as published by the Free Software Foundation.    *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU GPL                        *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               *
**************************************************************************/

#include <internal/configparser.hpp>
#include <cupt/file.hpp>
#include <cupt/regex.hpp>

namespace cupt {
namespace internal {

ConfigParser::ConfigParser(Handler regularHandler, Handler listHandler, Handler clearHandler)
	: __regular_handler(regularHandler), __list_handler(listHandler),
	__clear_handler(clearHandler)
{}

void ConfigParser::parse(const string& path)
{
	string openError;
	File file(path, "r", openError);
	if (!openError.empty())
	{
		fatal("unable to open file '%s': %s", path.c_str(), openError.c_str());
	}

	string block;
	file.getFile(block);

	static const sregex lineCommentRegex = sregex::compile("(?:#\\s|//).*$", regex_constants::not_dot_newline);
	block = regex_replace(block, lineCommentRegex, "");

	FORIT(charIt, block)
	{
		if (*charIt == '\n' || *charIt == '\t')
		{
			*charIt = ' ';
		}
	}

	__option_prefix = "";
	__errors.clear();
	__current = block.begin();
	__end = block.end();
	__skip_spaces();
	try
	{
		__statements();
	}
	catch (Exception&)
	{
		fatal("unable to parse config file '%s'", path.c_str());
	}
}

void ConfigParser::__statements()
{
	while (__statement()) {}
}

bool ConfigParser::__statement()
{
	if (__clear())
	{
		return true;
	}
	return __simple_or_nested_or_list();
}

bool ConfigParser::__simple_or_nested_or_list()
{
	if (!__name())
	{
		return false;
	}
	string name = __read;

	if (__value())
	{
		// simple
		string value = __read;
		__regular_handler(__option_prefix + name, value);
	}
	else
	{
		// nested or list
		if (!__opening_bracket())
		{
			__error_out();
		}

		// let's see if it's a list
		bool listFound = false;
		while (__value())
		{
			listFound = true;
			string value = __read;
			if (!__semicolon())
			{
				__error_out();
			}
			__list_handler(__option_prefix + name, value);
		}

		if (!listFound)
		{
			// so it should be a nested
			string oldOptionPrefix = __option_prefix;
			__option_prefix += name + "::";
			__statements();
			__option_prefix = oldOptionPrefix;
		}

		if (!__closing_bracket())
		{
			__error_out();
		}
	}

	if (!__semicolon())
	{
		__error_out();
	}
	return true;
}

bool ConfigParser::__clear()
{
	if (!__string("#clear"))
	{
		__maybe_error(__("clear directive ('#clear')"));
		return false;
	}
	if (!__name())
	{
		__error_out();
	}
	string name = __read;
	if (!__semicolon())
	{
		__error_out();
	}
	__clear_handler(__option_prefix + name, "");
	return true;
}

bool ConfigParser::__value()
{
	static const sregex regex = sregex::compile("\".*?\"", regex_constants::not_dot_newline);
	auto result = __regex(regex);
	if (!result)
	{
		__maybe_error(__("option value (quoted string)"));
	}
	return result;
}

bool ConfigParser::__name()
{
	static const sregex regex = sregex::compile("(?:[\\w/.-]+::)*[\\w/.-]+",
			regex_constants::not_dot_newline);
	auto result = __regex(regex);
	if (!result)
	{
		__maybe_error(__("option name (letters, numbers, slashes, points, dashes, double colons allowed)"));
	}
	return result;
}

bool ConfigParser::__semicolon()
{
	auto result = __string(";");
	if (!result)
	{
		__maybe_error(__("semicolon (';')"));
	}
	return result;
}

bool ConfigParser::__opening_bracket()
{
	auto result = __string("{");
	if (!result)
	{
		__maybe_error(__("opening curly bracket ('{')"));
	}
	return result;
}

bool ConfigParser::__closing_bracket()
{
	auto result = __string("}");
	if (!result)
	{
		__maybe_error(__("closing curly bracket ('}')"));
	}
	return result;
}

bool ConfigParser::__regex(const sregex& regex)
{
	smatch m;
	sci previous = __current;
	if (regex_search(__current, __end, m, regex, regex_constants::match_continuous))
	{
		// accepted the term
		__current = m[0].second;
		__read.assign(previous, __current);
		__skip_spaces();
		__errors.clear();
		return true;
	}
	else
	{
		return false;
	}
}

bool ConfigParser::__string(const char* str)
{
	ssize_t length = strlen(str);
	if (__end - __current < length)
	{
		return false;
	}
	auto result = memcmp(&*__current, str, length);
	if (result == 0)
	{
		__read.assign(__current, __current + length);
		__current += length;
		__skip_spaces();
		__errors.clear();
		return true;
	}
	else
	{
		return false;
	}
}

void ConfigParser::__skip_spaces()
{
	while (__current != __end && *__current == ' ')
	{
		++__current;
	}
}

void ConfigParser::__maybe_error(const string& message)
{
	__errors.push_back(message);
}

void ConfigParser::__error_out()
{
	string errorDescription = join(" or ", __errors);
	ssize_t contextLength = 40;
	if (__end - __current < contextLength)
	{
		contextLength = __end - __current;
	}
	string context(__current, __current + contextLength);

	fatal("expected: %s before '%s'", errorDescription.c_str(), context.c_str());
}

} // namespace
} // namespace
