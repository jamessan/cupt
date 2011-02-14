/**************************************************************************
*   Copyright (C) 2010-2011 by Eugene V. Lyubimkin                        *
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
#ifndef CUPT_INTERNAL_NATIVERESOLVER_DEPENDENCY_GRAPH_SEEN
#define CUPT_INTERNAL_NATIVERESOLVER_DEPENDENCY_GRAPH_SEEN

#include <map>
#include <forward_list>
using std::forward_list;

#include <cupt/common.hpp>
#include <cupt/fwd.hpp>
#include <cupt/system/resolver.hpp>
typedef cupt::system::Resolver::Reason Reason;
using cupt::cache::BinaryVersion;

#include <internal/graph.hpp>

namespace cupt {
namespace internal {

class PackageEntry;

namespace dependencygraph {

struct InitialPackageEntry
{
	shared_ptr< const BinaryVersion > version;
	bool sticked;
	bool modified;

	InitialPackageEntry();
};

struct BasicVertex;
typedef const BasicVertex* Element;
struct BasicVertex
{
	virtual string toString() const = 0;
	virtual size_t getPriority() const;
	virtual shared_ptr< const Reason > getReason(const BasicVertex& parent) const;
	virtual bool isAnti() const;
	virtual const forward_list< const Element* >* getRelatedElements() const;
};
struct VersionVertex: public BasicVertex
{
 private:
	const map< string, forward_list< const Element* > >::iterator __related_element_ptrs_it;
 public:
	shared_ptr< const BinaryVersion > version;

	VersionVertex(const map< string, forward_list< const Element* > >::iterator&);
	string toString() const;
	const forward_list< const Element* >* getRelatedElements() const;
	const string& getPackageName() const;
};
typedef const VersionVertex* VersionElement;

class DependencyGraph: protected Graph< Element >
{
	const Config& __config;
	const Cache& __cache;
	map< string, forward_list< const Element* > > __package_name_to_vertex_ptrs;
	map< string, const Element* > __empty_package_to_vertex_ptr;

	bool __can_package_be_removed(const string&,
			const map< string, shared_ptr< const BinaryVersion > >&) const;
 public:
	DependencyGraph(const Config& config, const Cache& cache);
	~DependencyGraph();
	vector< pair< const Element*, PackageEntry > > fill(
			const map< string, shared_ptr< const BinaryVersion > >&,
			const map< string, InitialPackageEntry >&);

	const Element* getCorrespondingEmptyElement(const Element*);
	using Graph< Element >::getSuccessorsFromPointer;
	using Graph< Element >::getPredecessorsFromPointer;
};

}
}
}

#endif
