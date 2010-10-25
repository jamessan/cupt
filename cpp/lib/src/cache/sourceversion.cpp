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
#include <cupt/cache/sourceversion.hpp>
#include <cupt/cache/releaseinfo.hpp>

#include <internal/tagparsemacro.hpp>
#include <internal/common.hpp>

namespace cupt {
namespace cache {

shared_ptr< SourceVersion > SourceVersion::parseFromFile(const Version::InitializationParameters& initParams)
{
	auto v = new SourceVersion;

	Source source;

	v->packageName = initParams.packageName;
	source.release = initParams.releaseInfo;

	v->priority = Version::Priorities::Extra; // default value if not specified

	{ // actual parsing
		// go to starting byte of the entry
		initParams.file->seek(initParams.offset);

		string block;
		smatch m;
		TagValue tagValue; // used in TAG() macro

		// read all version entry entirely
		initParams.file->getRecord(block);
		{ // parsing checksums and file names
			smatch lineMatch;
#define PARSE_CHECKSUM_RECORD(TagName, HashSumName) \
			TAG_CUSTOM( TagName , "*$(?:\\n)((?:^ .*$(?:\\n))*)", \
			{ \
				auto lines = split('\n', m[1]); \
				FORIT(lineIt, lines) \
				{ \
					const string& line = *lineIt; \
 \
					static const sregex checksumsLineRegex = sregex::compile(" ([[:xdigit:]]+) +(\\d+) +(.*)", regex_constants::optimize); \
					if (!regex_match(line, lineMatch, checksumsLineRegex)) \
					{ \
						fatal("malformed line '%s'", line.c_str()); \
					} \
					const string name = lineMatch[3]; \
 \
					static const sregex dscPartRegex = sregex::compile("\\.dsc$", regex_constants::optimize); \
					static const sregex diffPartRegex = sregex::compile("\\.(?:diff\\.gz|debian\\.tar\\.\\w+)$", regex_constants::optimize); \
					SourceVersion::FileParts::Type part = (regex_search(name, dscPartRegex) ? SourceVersion::FileParts::Dsc : \
							(regex_search(name, diffPartRegex) ? SourceVersion::FileParts::Diff : SourceVersion::FileParts::Tarball)); \
					bool foundRecord = false; \
					FORIT(recordIt, v->files[part]) \
					{ \
						if (recordIt->name == name) \
						{ \
							recordIt->hashSums[HashSums:: HashSumName ] = lineMatch[1]; \
							foundRecord = true; \
							break; \
						} \
					} \
\
					if (!foundRecord) \
					{ \
						SourceVersion::FileRecord& fileRecord = \
								(v->files[part].push_back(SourceVersion::FileRecord()), *(v->files[part].rbegin())); \
						fileRecord.name = name; \
						fileRecord.size = internal::string2uint32(lineMatch[2]); \
						fileRecord.hashSums[HashSums:: HashSumName ] = lineMatch[1]; \
					} \
				} \
			})

			PARSE_CHECKSUM_RECORD(Files, MD5)
			PARSE_CHECKSUM_RECORD(Checksums-Sha1, SHA1)
			PARSE_CHECKSUM_RECORD(Checksums-Sha256, SHA256)
#undef PARSE_CHECKSUM_RECORD
		}

		TAG_CUSTOM(Binary, "(.*(?:\\n^ .*$)*)",
		{
			static const sregex commaAndNewlineSeparatedRegex = sregex::compile("\\s*\\n?\\s*,\\s*\\n?\\s*", regex_constants::optimize);
			v->binaryPackageNames = split(commaAndNewlineSeparatedRegex, m[1]);
		})
		v->sources.push_back(source);
		TAG(Directory, v->sources[0].directory = tagValue;)
		TAG(Version, v->versionString = tagValue;)
		checkVersionString(v->versionString);
		PARSE_PRIORITY
		TAG(Architecture, v->architectures = split(' ', tagValue);)

		if (Version::parseRelations)
		{
			TAG(Build-Depends, v->relations[RelationTypes::BuildDepends] = ArchitecturedRelationLine(tagValue);)
			TAG(Build-Depends-Indep, v->relations[RelationTypes::BuildDependsIndep] = ArchitecturedRelationLine(tagValue);)
			TAG(Build-Conflicts, v->relations[RelationTypes::BuildConflicts] = ArchitecturedRelationLine(tagValue);)
			TAG(Build-Conflicts-Indep, v->relations[RelationTypes::BuildConflictsIndep] = ArchitecturedRelationLine(tagValue);)
		}

		if (Version::parseInfoOnly)
		{
			TAG(Section, v->section = tagValue;)
			TAG(Maintainer, v->maintainer = tagValue;)
			static const sregex commaSeparatedRegex = sregex::compile("\\s*,\\s*", regex_constants::optimize);
			TAG(Uploaders, v->uploaders = split(commaSeparatedRegex, tagValue);)
			PARSE_OTHERS
		}
	}

	if (v->versionString.empty())
	{
		fatal("version string isn't defined");
	}
	if (v->architectures.empty())
	{
		warn("source package %s, version %s: architectures aren't defined, setting them to 'all'",
				v->packageName.c_str(), v->versionString.c_str());
		v->architectures.push_back("all");
	}
	// no need to verify hash sums for emptyness, it's guarantted by parsing algorithm above

	return shared_ptr< SourceVersion >(v);
}

bool SourceVersion::areHashesEqual(const shared_ptr< const Version >& other) const
{
	shared_ptr< const SourceVersion > o = dynamic_pointer_cast< const SourceVersion >(other);
	if (!o)
	{
		fatal("internal error: areHashesEqual: non-source version parameter");
	}
	for (size_t i = 0; i < SourceVersion::FileParts::Count; ++i)
	{
		// not perfect algorithm, it requires the same order of files
		auto fileCount = files[i].size();
		auto otherFileCount = o->files[i].size();
		if (fileCount != otherFileCount)
		{
			return false;
		}
		for (size_t j = 0; j < fileCount; ++j)
		{
			if (! files[i][j].hashSums.match(o->files[i][j].hashSums))
			{
				return false;
			}
		}
	}
	return true;
}

const string SourceVersion::FileParts::strings[] = {
	__("Tarball"), __("Diff"), __("Dsc")
};
const string SourceVersion::RelationTypes::strings[] = {
	__("Build-Depends"), __("Build-Depends-Indep"), __("Build-Conflicts"), __("Build-Conflicts-Indep"),
};
const char* SourceVersion::RelationTypes::rawStrings[] = {
	"build-depends", "build-depend-indep", "build-conflicts", "build-conflicts-indep",
};

}
}
