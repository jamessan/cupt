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

#include <cstdlib>

#include <cupt/file.hpp>
#include <cupt/regex.hpp>
#include <cupt/cache/binaryversion.hpp>
#include <cupt/cache/releaseinfo.hpp>

#include <internal/tagparsemacro.hpp>
#include <internal/common.hpp>

namespace cupt {
namespace cache {

shared_ptr< BinaryVersion > BinaryVersion::parseFromFile(const Version::InitializationParameters& initParams)
{
	auto v = new BinaryVersion;

	Source source;

	v->essential = false;
	v->packageName = initParams.packageName;
	source.release = initParams.releaseInfo;

	v->installedSize = 0;
	v->priority = Version::Priorities::Extra; // default value if not specified
	v->file.size = 0;

	{ // actual parsing
		// go to starting byte of the entry
		initParams.file->seek(initParams.offset);

		string block;
		TagValue tagValue; // used in TAG() macro

		static auto rejectSpaceStarted = [](const char* buf, size_t) -> bool
		{
			return !isspace(buf[0]);
		};
		static auto acceptAll = [](const char*, size_t) -> bool { return true; };

		// read all version entry entirely
		initParams.file->getRecord(block, parseInfoOnly ? acceptAll : rejectSpaceStarted);

		TAG(Version, v->versionString = tagValue;)
		checkVersionString(v->versionString);
		TAG(Essential, v->essential = (string(tagValue) == "yes");)
		PARSE_PRIORITY
		TAG(Size, v->file.size = internal::string2uint32(tagValue);)
		TAG(Installed-Size, v->installedSize = internal::string2uint32(tagValue) * 1024;)
		TAG(Architecture, v->architecture = tagValue;)
		TAG(Filename,
		{
			string filename = tagValue;
			auto lastSlashPosition = filename.find_last_of('/');
			if (lastSlashPosition == string::npos)
			{
				// source.directory remains empty
				v->file.name = filename;
			}
			else
			{
				source.directory = filename.substr(0, lastSlashPosition);
				v->file.name = filename.substr(lastSlashPosition + 1);
			}
		})
		TAG(MD5sum, v->file.hashSums[HashSums::MD5] = tagValue;)
		TAG(SHA1, v->file.hashSums[HashSums::SHA1] = tagValue;)
		TAG(SHA256, v->file.hashSums[HashSums::SHA256] = tagValue;)
		TAG(Task, v->task = tagValue;)
		TAG(Source,
		{
			v->sourcePackageName = tagValue;
			string& value = v->sourcePackageName;
			// determing do we have source version appended or not?
			// example: "abcd (1.2-5)"
			auto size = value.size();
			if (size > 2 && value[size-1] == ')')
			{
				auto delimiterPosition = value.rfind('(', size-2);
				if (delimiterPosition != string::npos)
				{
					// found! there is a source version, most probably
					// indicating that it was some binary-only rebuild, and
					// the source version is different with binary one
					v->sourceVersionString = value.substr(delimiterPosition+1, size-delimiterPosition-2);
					checkVersionString(v->sourceVersionString);
					if (delimiterPosition != 0 && value[delimiterPosition-1] == ' ')
					{
						--delimiterPosition;
					}
					v->sourcePackageName.erase(delimiterPosition);
				}
			}
		};)

		if (Version::parseRelations)
		{
			TAG(Pre-Depends, v->relations[RelationTypes::PreDepends] = RelationLine(tagValue);)
			TAG(Depends, v->relations[RelationTypes::Depends] = RelationLine(tagValue);)
			TAG(Recommends, v->relations[RelationTypes::Recommends] = RelationLine(tagValue);)
			TAG(Suggests, v->relations[RelationTypes::Suggests] = RelationLine(tagValue);)
			TAG(Conflicts, v->relations[RelationTypes::Conflicts] = RelationLine(tagValue);)
			TAG(Breaks, v->relations[RelationTypes::Breaks] = RelationLine(tagValue);)
			TAG(Replaces, v->relations[RelationTypes::Replaces] = RelationLine(tagValue);)
			TAG(Enhances, v->relations[RelationTypes::Enhances] = RelationLine(tagValue);)
			TAG(Provides,
			{
				auto callback = [&v](string::const_iterator begin, string::const_iterator end)
				{
					v->provides.push_back(string(begin, end));
				};
				internal::processSpaceCommaSpaceDelimitedStrings(tagValue.first, tagValue.second, callback);
			})
		}

		if (Version::parseInfoOnly)
		{
			TAG(Section, v->section = tagValue;)
			TAG(Maintainer, v->maintainer = tagValue;)
			TAG_MULTILINE(Description,
			{
				v->shortDescription = tagValue;
				v->longDescription = remainder;
			};)
			TAG(Tag, v->tags = tagValue;)
			PARSE_OTHERS
		}

		if (v->sourceVersionString.empty())
		{
			v->sourceVersionString = v->versionString;
		}
		if (v->sourcePackageName.empty())
		{
			v->sourcePackageName = v->packageName;
		}
	};

	if (v->versionString.empty())
	{
		fatal("version string isn't defined");
	}
	if (v->architecture.empty())
	{
		warn("package %s, version %s: architecture isn't defined, setting it to 'all'",
				v->packageName.c_str(), v->versionString.c_str());
		v->architecture = "all";
	}
	v->sources.push_back(source);
	if (!v->isInstalled() && v->file.hashSums.empty())
	{
		fatal("no hash sums specified");
	}

	return shared_ptr< BinaryVersion >(v);
}

bool BinaryVersion::isInstalled() const
{
	return sources[0].release->baseUri.empty();
}

bool BinaryVersion::areHashesEqual(const shared_ptr< const Version >& other) const
{
	shared_ptr< const BinaryVersion > o = dynamic_pointer_cast< const BinaryVersion >(other);
	if (!o)
	{
		fatal("internal error: areHashesEqual: non-binary version parameter");
	}
	return file.hashSums.match(o->file.hashSums);
}

const string BinaryVersion::RelationTypes::strings[] = {
	__("Pre-Depends"), __("Depends"), __("Recommends"), __("Suggests"),
	__("Enhances"), __("Conflicts"), __("Breaks"), __("Replaces")
};
const char* BinaryVersion::RelationTypes::rawStrings[] = {
	"pre-depends", "depends", "recommends", "suggests",
	"enhances", "conflicts", "breaks", "replaces"
};

}
}
