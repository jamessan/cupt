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
#ifndef CUPT_INTERNAL_SOLUTION_SEEN
#define CUPT_INTERNAL_SOLUTION_SEEN

#include <bitset>
#include <map>
#include <forward_list>
#include <cstring>

#include <cupt/cache/binaryversion.hpp>
#include <cupt/system/resolver.hpp>

#include <internal/nativeresolver/dependencygraph.hpp>

namespace cupt {
namespace internal {

namespace dg = dependencygraph;
typedef dg::DependencyGraph::CessorListType GraphCessorListType;

using namespace cache;
using namespace system;

using std::map;
using std::forward_list;

struct PackageEntry
{
	struct IntroducedBy
	{
		const dg::Element* versionElementPtr;
		const dg::Element* brokenElementPtr;

		IntroducedBy() : versionElementPtr(NULL) {}
		bool empty() const { return !versionElementPtr; }
		bool operator<(const IntroducedBy& other) const
		{
			return std::memcmp(this, &other, sizeof(*this)) < 0;
		}
		shared_ptr< const Resolver::Reason > getReason() const
		{
			return brokenElementPtr->getReason(*versionElementPtr);
		}
	};
	struct BrokenSuccessor
	{
		const dg::Element* elementPtr;
		size_t priority;

		BrokenSuccessor()
		{}
		BrokenSuccessor(const dg::Element* elementPtr_, size_t priority_)
			: elementPtr(elementPtr_), priority(priority_)
		{}
	};

	bool sticked;
	bool autoremoved;
	forward_list< BrokenSuccessor > brokenSuccessors;
	forward_list< const dg::Element* > rejectedConflictors;
	IntroducedBy introducedBy;

	PackageEntry();
	PackageEntry(PackageEntry&&) = default;
	PackageEntry(const PackageEntry&) = default;

	PackageEntry& operator=(PackageEntry&&) = default;
	PackageEntry& operator=(const PackageEntry&) = default;

	bool isModificationAllowed(const dg::Element*) const;
};

class PackageEntryMap;
class PackageEntrySet;

class Solution
{
	friend class SolutionStorage;
	friend class iterator;

	shared_ptr< const Solution > __parent;
	shared_ptr< const PackageEntryMap > __master_entries;
	shared_ptr< PackageEntryMap > __added_entries;
	shared_ptr< PackageEntrySet > __removed_entries;
 public:
	size_t id;
	size_t level;
	bool finished;
	ssize_t score;
	std::unique_ptr< const void > pendingAction;
	vector< const dg::Element* > insertedElementPtrs; // in time order

	Solution();
	Solution(const Solution&) = delete;
	Solution& operator=(const Solution&) = delete;

	void prepare();
	vector< const dg::Element* > getElements() const;
	// result becomes invalid after any setPackageEntry
	const PackageEntry* getPackageEntry(const dg::Element*) const;

	class const_iterator
	{
		const pair< const dg::Element*, PackageEntry >* __master_it;
		decltype(__master_it) __master_end;
		decltype(__master_it) __added_it;
		decltype(__master_it) __added_end;
		const Solution& __solution;
		enum class State;
	    State __state;
		friend class Solution;

		inline void __init();
		inline void __move();
		inline bool __is_step_completed() const;
	 public:
		typedef pair< const dg::Element*, PackageEntry > value_t;
		typedef const_iterator self;

		const_iterator(const Solution&);
		bool operator==(const self&) const;
		bool operator!=(const self&) const;
		self& operator++();
		const value_t& operator*() const;
		const value_t* operator->() const;
	};
	const_iterator begin() const;
	const_iterator end() const;
};

class SolutionStorage
{
	size_t __next_free_id;
	dg::DependencyGraph __dependency_graph;
 public:
	SolutionStorage(const Config&, const Cache& cache);
	shared_ptr< Solution > cloneSolution(const shared_ptr< Solution >&);
	void prepareForResolving(Solution&,
			const map< string, shared_ptr< const BinaryVersion > >&,
			const map< string, dg::InitialPackageEntry >&);
	const dg::Element* getCorrespondingEmptyElement(const dg::Element*);
	const GraphCessorListType& getSuccessorElements(const dg::Element*) const;
	const GraphCessorListType& getPredecessorElements(const dg::Element*) const;
	bool verifyElement(const Solution&, const dg::Element*) const;

	// may include parameter itself
	static const forward_list< const dg::Element* >&
			getConflictingElements(const dg::Element*);
	bool simulateSetPackageEntry(const Solution& solution,
			const dg::Element*, const dg::Element**) const;
	void setRejection(Solution&, const dg::Element*);
	void setPackageEntry(Solution&, const dg::Element*,
			PackageEntry&&, const dg::Element*);
	void unfoldElement(const dg::Element*);
};

}
}

#endif

